#include "ModbusRtuService.h"
#include "ModbusRegisterMap.h"
#include "ResourceMonitor.h"

using namespace ModbusRegisterMap;

namespace {

// parity codes for HardwareSerial::begin()
constexpr uint32_t kSerialConfig[][3] = {
    // {stopBits=1, stopBits=2}  parity: 0=N 1=E 2=O
    {SERIAL_8N1, SERIAL_8E1, SERIAL_8O1},  // stop=1
    {SERIAL_8N2, SERIAL_8E2, SERIAL_8O2},  // stop=2
};

uint32_t serialConfig(uint8_t parity, uint8_t stopBits) {
    const uint8_t si = (stopBits == 2) ? 1 : 0;
    const uint8_t pi = (parity  >  2)  ? 0 : parity;
    return kSerialConfig[si][pi];
}

uint16_t expectedRequestLength(const uint8_t* buf, uint16_t len) {
    if (len < 2) return 0;
    switch (buf[1]) {
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
            return 8;
        case 0x10:
            if (len < 7) return 0;
            return static_cast<uint16_t>(9 + buf[6]);
        default:
            return 0;
    }
}

}  // namespace

ModbusRtuService::ModbusRtuService(StatusStore& statusStore, SettingsStore& settingsStore,
                                    FillController& fillController, TransactionLog& transactionLog,
                                    RtcService& rtcService, ModbusRegisterCache& registerCache)
    : statusStore_(statusStore),
      settingsStore_(settingsStore),
      fillController_(fillController),
      transactionLog_(transactionLog),
      rtcService_(rtcService),
      registerCache_(registerCache),
      uart_(Serial2) {}

void ModbusRtuService::begin() {
    reloadSettings();
    if (!cachedEnabled_) {
        Serial.println(F("[RTU] Modbus RTU disabled"));
        return;
    }

    uart_.setRxBufferSize(1024);
    uart_.setTxBufferSize(512);

    const uint32_t cfg = serialConfig(cachedParity_, cachedStopBits_);
    uart_.begin(cachedBaud_, cfg, BoardConfig::kRtuRxPin, BoardConfig::kRtuTxPin);

    if (BoardConfig::kRtuDePin != 255) {
        pinMode(BoardConfig::kRtuDePin, OUTPUT);
        digitalWrite(BoardConfig::kRtuDePin, LOW);  // receive mode
    }

    active_ = true;
    const char* src;
    switch (settingsStore_.rtuConfigSource()) {
        case RtuConfigSource::NVS:            src = "NVS"; break;
        case RtuConfigSource::FactoryDefault: src = "FACTORY_DEFAULT"; break;
        default:                              src = "INVALID_FALLBACK"; break;
    }
    Serial.printf("[RTU] RTU config active: slave=%u baud=%u parity=%u stop=%u source=%s\n",
                  cachedSlaveAddr_, cachedBaud_, cachedParity_, cachedStopBits_, src);
    if (settingsStore_.rtuConfigSource() == RtuConfigSource::NVS &&
        cachedBaud_ != 115200) {
        Serial.println(F("[RTU] Note: active baud is NOT 115200 (saved NVS value). "
                         "Use Apply Recommended 115200 on the Modbus page to upgrade."));
    }
}

void ModbusRtuService::handleClient() {
    if (!active_) return;

    while (uart_.available()) {
        if (rxLen_ < kRxBufSize) {
            rxBuf_[rxLen_++] = static_cast<uint8_t>(uart_.read());
        } else {
            uart_.read(); // overflow - discard
            ResourceMonitor::instance().incrementRtuError();
        }
        lastByteMs_ = millis();
    }

    while (true) {
        while (rxLen_ > 0 && rxBuf_[0] != cachedSlaveAddr_ && rxBuf_[0] != kBroadcastAddr) {
            memmove(rxBuf_, rxBuf_ + 1, rxLen_ - 1);
            rxLen_--;
            ResourceMonitor::instance().incrementRtuError();
        }

        const uint16_t expected = expectedRequestLength(rxBuf_, rxLen_);
        if (expected == 0 || rxLen_ < expected) break;

        const uint16_t rxCrc = static_cast<uint16_t>(rxBuf_[expected - 2]) |
                               (static_cast<uint16_t>(rxBuf_[expected - 1]) << 8);
        const uint16_t calcCrc = crc16(rxBuf_, expected - 2);
        if (rxCrc != calcCrc) {
            memmove(rxBuf_, rxBuf_ + 1, rxLen_ - 1);
            rxLen_--;
            ResourceMonitor::instance().incrementRtuError();
            continue;
        }

        const uint16_t savedLen = rxLen_;
        rxLen_ = expected;
        processFrame();

        const uint16_t remaining = static_cast<uint16_t>(savedLen - expected);
        if (remaining > 0) {
            memmove(rxBuf_, rxBuf_ + expected, remaining);
        }
        rxLen_ = remaining;
    }

    // Frame ends after kFrameGapMs silence and minimum 4 bytes (addr+FC+CRC16)
    if (rxLen_ >= 4 && millis() - lastByteMs_ >= kFrameGapMs) {
        processFrame();
        rxLen_ = 0;
    }
}

void ModbusRtuService::processFrame() {
    const uint8_t addr = rxBuf_[0];

    // Ignore frames not addressed to us (broadcast 0 is still handled but no response sent)
    if (addr != cachedSlaveAddr_ && addr != kBroadcastAddr) return;

    // Validate CRC — last two bytes are CRC16 LE
    const uint16_t rxCrc    = static_cast<uint16_t>(rxBuf_[rxLen_ - 2]) |
                              (static_cast<uint16_t>(rxBuf_[rxLen_ - 1]) << 8);
    const uint16_t calcCrc  = crc16(rxBuf_, rxLen_ - 2);
    if (rxCrc != calcCrc) {
#if LPG_RTU_DEBUG
        Serial.printf("[RTU] CRC error: rx=0x%04X calc=0x%04X\n", rxCrc, calcCrc);
#endif
        ResourceMonitor::instance().incrementRtuError();
        return;
    }

    const uint8_t fc      = rxBuf_[1];
    const uint8_t* pduReq = &rxBuf_[2];
    const uint16_t pduLen = rxLen_ - 4; // strip addr + FC + CRC

    uint8_t  respBuf[256 + 5]; // addr(1) + FC(1) + data(≤252) + CRC(2)
    uint16_t respPduLen = 0;

    respBuf[0] = cachedSlaveAddr_; // echo address
    const uint32_t t0 = micros();
    bool ok = dispatchFC(fc, pduReq, pduLen, &respBuf[2], respPduLen);
    ResourceMonitor::instance().recordRtuTiming(micros() - t0);

    if (!ok) {
        ResourceMonitor::instance().incrementRtuError();
        // Exception response: addr(1) + (FC|0x80)(1) + exCode(1) + CRC(2)
        // respBuf[2] already holds the exception code written by the handler via resp[0]
        respBuf[1] = fc | 0x80;
        const uint16_t excCrc = crc16(respBuf, 3);
        respBuf[3] = static_cast<uint8_t>(excCrc & 0xFF);
        respBuf[4] = static_cast<uint8_t>(excCrc >> 8);
        if (addr != kBroadcastAddr) sendResponse(respBuf, 5);
        return;
    }

    ResourceMonitor::instance().incrementRtuRequest();

    respBuf[1] = fc;
    const uint16_t totalLen = 2 + respPduLen; // addr + FC + pdu
    const uint16_t crc      = crc16(respBuf, totalLen);
    respBuf[totalLen]     = static_cast<uint8_t>(crc & 0xFF);
    respBuf[totalLen + 1] = static_cast<uint8_t>(crc >> 8);

    if (addr != kBroadcastAddr) sendResponse(respBuf, totalLen + 2);

    // If RTU config registers were written this frame, reinitialize UART now —
    // after the success response has been fully sent at the old baud rate.
    if (pendingUartReinit_) {
        pendingUartReinit_ = false;
        uart_.flush();
        delayMicroseconds(2000);  // ~2 ms: ensure last byte clears shift register
        uart_.end();
        const uint32_t cfg = serialConfig(cachedParity_, cachedStopBits_);
        uart_.begin(cachedBaud_, cfg, BoardConfig::kRtuRxPin, BoardConfig::kRtuTxPin);
        uart_.setRxBufferSize(1024);
        uart_.setTxBufferSize(512);
        Serial.printf("[RTU] UART reinitialized: baud=%u parity=%u stop=%u\n",
                      cachedBaud_, cachedParity_, cachedStopBits_);
    }
}

void ModbusRtuService::sendResponse(const uint8_t* buf, uint16_t len) {
    if (BoardConfig::kRtuDePin != 255) {
        digitalWrite(BoardConfig::kRtuDePin, HIGH); // transmit mode
        delayMicroseconds(50);
    }
    uart_.write(buf, len);
    uart_.flush();
    if (BoardConfig::kRtuDePin != 255) {
        delayMicroseconds(50);
        digitalWrite(BoardConfig::kRtuDePin, LOW);  // receive mode
    }
}

bool ModbusRtuService::dispatchFC(uint8_t fc, const uint8_t* req, uint16_t reqLen,
                                   uint8_t* resp, uint16_t& respLen) {
    switch (fc) {
        case 0x01: return handleFC01(req, resp, respLen);
        case 0x02: return handleFC02(req, resp, respLen);
        case 0x03: return handleFC03(req, resp, respLen);
        case 0x04: return handleFC03(req, resp, respLen); // FC04 mirrors FC03
        case 0x05: return handleFC05(req, resp, respLen);
        case 0x06: return handleFC06(req, resp, respLen);
        case 0x10: return handleFC16(req, resp, respLen);
        default:
            resp[0] = 0x01; // Illegal Function
            respLen = 1;
            return false;
    }
}

// ── FC01: Read Coils (from cache) ─────────────────────────────────────────────
bool ModbusRtuService::handleFC01(const uint8_t* req, uint8_t* resp, uint16_t& respLen) {
    const uint16_t start = readU16BE(req);
    const uint16_t qty   = readU16BE(req + 2);
    if (qty < 1 || qty > 2000 || start + qty > kCoil_Count) {
        resp[0] = (qty < 1 || qty > 2000) ? 0x03 : 0x02;
        respLen = 1;
        return false;
    }
    const uint8_t byteCount = static_cast<uint8_t>((qty + 7) / 8);
    resp[0] = byteCount;
    for (uint8_t i = 0; i < byteCount; i++) resp[1 + i] = 0;
    for (uint16_t i = 0; i < qty; i++) {
        if (registerCache_.readCoil(static_cast<uint16_t>(start + i)))
            resp[1 + i / 8] |= (1 << (i % 8));
    }
    respLen = 1 + byteCount;
    return true;
}

// ── FC02: Read Discrete Inputs (from cache) ───────────────────────────────────
bool ModbusRtuService::handleFC02(const uint8_t* req, uint8_t* resp, uint16_t& respLen) {
    const uint16_t start = readU16BE(req);
    const uint16_t qty   = readU16BE(req + 2);
    if (qty < 1 || qty > 2000 || start + qty > kDI_Count) {
        resp[0] = (qty < 1 || qty > 2000) ? 0x03 : 0x02;
        respLen = 1;
        return false;
    }
    const uint8_t byteCount = static_cast<uint8_t>((qty + 7) / 8);
    resp[0] = byteCount;
    for (uint8_t i = 0; i < byteCount; i++) resp[1 + i] = 0;
    for (uint16_t i = 0; i < qty; i++) {
        if (registerCache_.readDI(static_cast<uint16_t>(start + i)))
            resp[1 + i / 8] |= (1 << (i % 8));
    }
    respLen = 1 + byteCount;
    return true;
}

// ── FC03: Read Holding Registers (reads from RAM register cache) ───────────────
bool ModbusRtuService::handleFC03(const uint8_t* req, uint8_t* resp, uint16_t& respLen) {
    const uint16_t startAddr = readU16BE(req);
    const uint16_t qty       = readU16BE(req + 2);
    if (qty < 1 || qty > 125) {
        resp[0] = 0x03; respLen = 1; return false;
    }
    if (startAddr < kHR_Base || startAddr + qty > kHR_Base + kHR_Count) {
        resp[0] = 0x02; respLen = 1; return false;
    }
    resp[0] = static_cast<uint8_t>(qty * 2);
    registerCache_.readBlock(static_cast<uint16_t>(startAddr - kHR_Base), qty, &resp[1]);
    respLen = 1 + qty * 2;
    return true;
}

// ── FC05: Write Single Coil ───────────────────────────────────────────────────
bool ModbusRtuService::handleFC05(const uint8_t* req, uint8_t* resp, uint16_t& respLen) {
    const uint16_t coilAddr  = readU16BE(req);
    const uint16_t coilValue = readU16BE(req + 2);
    if (coilValue != 0x0000 && coilValue != 0xFF00) {
        resp[0] = 0x03; respLen = 1; return false;
    }
    if (coilAddr >= kCoil_Count) {
        resp[0] = 0x02; respLen = 1; return false;
    }
    if (!writeCoil(coilAddr, coilValue == 0xFF00, statusStore_)) {
        resp[0] = 0x02; respLen = 1; return false;
    }
    resp[0] = req[0]; resp[1] = req[1]; resp[2] = req[2]; resp[3] = req[3];
    respLen = 4;
    return true;
}

// ── FC06: Write Single Register ───────────────────────────────────────────────
bool ModbusRtuService::handleFC06(const uint8_t* req, uint8_t* resp, uint16_t& respLen) {
    const uint16_t regAddr  = readU16BE(req);
    const uint16_t regValue = readU16BE(req + 2);
    if (regAddr < kHR_Base || regAddr >= kHR_Base + kHR_Count) {
        resp[0] = 0x02; respLen = 1; return false;
    }
    const uint16_t regIdx = static_cast<uint16_t>(regAddr - kHR_Base);
    if (!writeHR(regIdx, regValue,
                 statusStore_, settingsStore_, fillController_, rtcService_)) {
        resp[0] = 0x03; respLen = 1; return false;
    }
    registerCache_.updateFast(statusStore_.snapshot(), settingsStore_, mqttConnected_);
    if (regIdx >= kHR_RtuSlaveAddr && regIdx <= kHR_RtuStopBits) {
        const uint32_t oldBaud = cachedBaud_; const uint8_t oldSlave = cachedSlaveAddr_;
        const uint8_t oldPar  = cachedParity_; const uint8_t oldStop = cachedStopBits_;
        reloadSettings();
        if (cachedBaud_ != oldBaud || cachedSlaveAddr_ != oldSlave ||
            cachedParity_ != oldPar || cachedStopBits_ != oldStop) {
            pendingUartReinit_ = true;
        }
    }
    resp[0] = req[0]; resp[1] = req[1]; resp[2] = req[2]; resp[3] = req[3];
    respLen = 4;
    return true;
}

// ── FC16: Write Multiple Registers ───────────────────────────────────────────
bool ModbusRtuService::handleFC16(const uint8_t* req, uint8_t* resp, uint16_t& respLen) {
    const uint16_t startAddr = readU16BE(req);
    const uint16_t qty       = readU16BE(req + 2);
    const uint8_t  byteCount = req[4];
    if (qty < 1 || qty > 123 || byteCount != qty * 2) {
        resp[0] = 0x03; respLen = 1; return false;
    }
    if (startAddr < kHR_Base || startAddr + qty > kHR_Base + kHR_Count) {
        resp[0] = 0x02; respLen = 1; return false;
    }
    const uint8_t* data = req + 5;
    bool rtuCfgChanged = false;
    for (uint16_t i = 0; i < qty; i++) {
        const uint16_t addr = static_cast<uint16_t>(startAddr + i - kHR_Base);
        const uint16_t val  = (static_cast<uint16_t>(data[i*2]) << 8) | data[i*2+1];
        if (!writeHR(addr, val, statusStore_, settingsStore_, fillController_, rtcService_)) {
            resp[0] = 0x03;
            respLen = 1;
            return false;
        }
        if (addr >= kHR_RtuSlaveAddr && addr <= kHR_RtuStopBits) rtuCfgChanged = true;
    }
    registerCache_.updateFast(statusStore_.snapshot(), settingsStore_, mqttConnected_);
    if (rtuCfgChanged) {
        const uint32_t oldBaud = cachedBaud_; const uint8_t oldSlave = cachedSlaveAddr_;
        const uint8_t oldPar  = cachedParity_; const uint8_t oldStop = cachedStopBits_;
        reloadSettings();
        if (cachedBaud_ != oldBaud || cachedSlaveAddr_ != oldSlave ||
            cachedParity_ != oldPar || cachedStopBits_ != oldStop) {
            pendingUartReinit_ = true;
        }
    }
    resp[0] = req[0]; resp[1] = req[1]; resp[2] = req[2]; resp[3] = req[3];
    respLen = 4;
    return true;
}

// ── CRC16 (Modbus polynomial 0xA001) ─────────────────────────────────────────
uint16_t ModbusRtuService::crc16(const uint8_t* data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) { crc >>= 1; crc ^= 0xA001; }
            else               { crc >>= 1; }
        }
    }
    return crc;
}

uint16_t ModbusRtuService::readU16BE(const uint8_t* p) {
    return (static_cast<uint16_t>(p[0]) << 8) | p[1];
}

void ModbusRtuService::reloadSettings() {
    const ModbusRtuSettings rtu = settingsStore_.rtuSnapshot();
    cachedEnabled_   = rtu.enabled;
    cachedSlaveAddr_ = rtu.slaveAddress;
    cachedBaud_      = rtu.baudRate;
    cachedParity_    = rtu.parity;
    cachedStopBits_  = rtu.stopBits;
}
