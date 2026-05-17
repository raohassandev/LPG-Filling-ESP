#include "ModbusTcpService.h"
#include "ModbusRegisterMap.h"
#include "ResourceMonitor.h"

// ────────────────────────────────────────────────────────────────────────────
//  Modbus TCP service — FC01/02/03/04/05/06/16
//
//  Frame layout:
//    MBAP header (7 bytes):  TID(2) PID(2) Length(2) UnitID(1)
//    PDU           (≥1 byte): FunctionCode(1) [data…]
//  Total minimum frame = 8 bytes.
//
//  Exception codes:
//    0x01  Illegal Function
//    0x02  Illegal Data Address
//    0x03  Illegal Data Value
// ────────────────────────────────────────────────────────────────────────────

namespace {

constexpr uint8_t kFC_ReadCoils       = 0x01;
constexpr uint8_t kFC_ReadDI          = 0x02;
constexpr uint8_t kFC_ReadHR          = 0x03;
constexpr uint8_t kFC_ReadIR          = 0x04;  // mirrors FC03
constexpr uint8_t kFC_WriteCoil       = 0x05;
constexpr uint8_t kFC_WriteHR         = 0x06;
constexpr uint8_t kFC_WriteMultiHR    = 0x10;

constexpr uint8_t kEx_IllegalFunc  = 0x01;
constexpr uint8_t kEx_IllegalAddr  = 0x02;
constexpr uint8_t kEx_IllegalValue = 0x03;

uint16_t readU16(const uint8_t* p) {
    return (static_cast<uint16_t>(p[0]) << 8) | p[1];
}

void writeU16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v & 0xFF);
}

// Fill bytes 0–6 of a response buffer with the MBAP header.
// afterLen = number of bytes that follow this header (UnitID counts as 1 of those).
void setMbap(uint8_t* buf, const uint8_t* req, uint16_t afterLen) {
    buf[0] = req[0]; buf[1] = req[1];  // Transaction ID (echo)
    buf[2] = 0x00;   buf[3] = 0x00;   // Protocol ID = 0
    buf[4] = static_cast<uint8_t>(afterLen >> 8);
    buf[5] = static_cast<uint8_t>(afterLen & 0xFF);
    buf[6] = req[6];                   // Unit ID (echo)
}

}  // namespace

// ── Constructor ───────────────────────────────────────────────────────────
ModbusTcpService::ModbusTcpService(StatusStore& statusStore, SettingsStore& settingsStore,
                                    FillController& fillController, TransactionLog& transactionLog,
                                    RtcService& rtcService, ModbusRegisterCache& registerCache)
    : statusStore_(statusStore),
      settingsStore_(settingsStore),
      fillController_(fillController),
      transactionLog_(transactionLog),
      rtcService_(rtcService),
      registerCache_(registerCache) {}

// ── begin ─────────────────────────────────────────────────────────────────
void ModbusTcpService::begin() {
    server_.begin();
    server_.setNoDelay(true);
    Serial.println(F("[MODBUS] TCP server started on port 502"));
    Serial.printf("[MODBUS] HR 0x%04X–0x%04X (%u)  Coils 0x%04X–0x%04X (%u)  DI 0x%04X–0x%04X (%u)\n",
        ModbusRegisterMap::kHR_Base,
        ModbusRegisterMap::kHR_Base + ModbusRegisterMap::kHR_Count - 1,
        ModbusRegisterMap::kHR_Count,
        ModbusRegisterMap::kCoil_Base,
        ModbusRegisterMap::kCoil_Base + ModbusRegisterMap::kCoil_Count - 1,
        ModbusRegisterMap::kCoil_Count,
        ModbusRegisterMap::kDI_Base,
        ModbusRegisterMap::kDI_Base + ModbusRegisterMap::kDI_Count - 1,
        ModbusRegisterMap::kDI_Count);
}

// ── handleClient (called every loop) ─────────────────────────────────────
void ModbusTcpService::handleClient() {
    // Accept new connection only when idle
    if (!activeClient_ || !activeClient_.connected()) {
        WiFiClient incoming = server_.available();
        if (incoming) {
            activeClient_ = incoming;
            activeClient_.setNoDelay(true);
            rxLen_ = 0;
        }
    }

    if (!activeClient_ || !activeClient_.connected()) return;

    // Drain available bytes into receive buffer
    while (activeClient_.available() > 0 && rxLen_ < static_cast<uint16_t>(sizeof(rxBuf_))) {
        rxBuf_[rxLen_++] = static_cast<uint8_t>(activeClient_.read());
    }

    // Need at least 8 bytes: 7-byte MBAP + FC
    if (rxLen_ < 8) return;

    // Validate Protocol ID (must be 0x0000 = Modbus)
    if (rxBuf_[2] != 0x00 || rxBuf_[3] != 0x00) {
        activeClient_.stop();
        rxLen_ = 0;
        return;
    }

    const uint16_t declaredLen = readU16(&rxBuf_[4]);       // bytes after MBAP prefix
    const uint16_t totalFrame  = static_cast<uint16_t>(6 + declaredLen);

    if (totalFrame > static_cast<uint16_t>(sizeof(rxBuf_))) {
        activeClient_.stop();
        rxLen_ = 0;
        return;
    }

    if (rxLen_ < totalFrame) return;  // incomplete frame — wait

    // Dispatch complete frame
    const uint8_t  fc     = rxBuf_[7];
    const uint8_t* pdu    = &rxBuf_[7];
    const uint16_t pduLen = static_cast<uint16_t>(declaredLen - 1);  // strip UnitID byte

    // Any valid Modbus TCP frame proves the HMI/client is alive.
    if (hmiService_) hmiService_->notifyModbusActivity();

    const uint32_t t0 = micros();
    dispatchPdu(activeClient_, rxBuf_, fc, pdu, pduLen);
    ResourceMonitor::instance().recordTcpTiming(micros() - t0);
    ResourceMonitor::instance().incrementTcpRequest();

    // Consume processed frame, keep any trailing bytes
    const uint16_t remaining = static_cast<uint16_t>(rxLen_ - totalFrame);
    if (remaining > 0) memmove(rxBuf_, rxBuf_ + totalFrame, remaining);
    rxLen_ = remaining;
}

// ── dispatchPdu ───────────────────────────────────────────────────────────
void ModbusTcpService::dispatchPdu(WiFiClient& client, const uint8_t* mbap, uint8_t fc,
                                    const uint8_t* pdu, uint16_t pduLen) {
    // FC01/02/03/04/05/06 all need at least 4 bytes of PDU data (addr+qty/value)
    const uint16_t startAddr = (pduLen >= 4) ? readU16(&pdu[1]) : 0;
    const uint16_t word2     = (pduLen >= 4) ? readU16(&pdu[3]) : 0;

    switch (fc) {
        case kFC_ReadCoils:
            if (pduLen < 4) { sendException(client, mbap, fc, kEx_IllegalFunc); return; }
            handleFC01(client, mbap, startAddr, word2);
            break;
        case kFC_ReadDI:
            if (pduLen < 4) { sendException(client, mbap, fc, kEx_IllegalFunc); return; }
            handleFC02(client, mbap, startAddr, word2);
            break;
        case kFC_ReadHR:
        case kFC_ReadIR:
            if (pduLen < 4) { sendException(client, mbap, fc, kEx_IllegalFunc); return; }
            handleFC03(client, mbap, fc, startAddr, word2);
            break;
        case kFC_WriteCoil:
            if (pduLen < 4) { sendException(client, mbap, fc, kEx_IllegalFunc); return; }
            handleFC05(client, mbap, startAddr, word2);
            break;
        case kFC_WriteHR:
            if (pduLen < 4) { sendException(client, mbap, fc, kEx_IllegalFunc); return; }
            handleFC06(client, mbap, startAddr, word2);
            break;
        case kFC_WriteMultiHR:
            // FC16 PDU: FC(1)+addr(2)+qty(2)+byteCount(1)+data(qty*2) — need ≥8 bytes for 1 register
            if (pduLen < 8) { sendException(client, mbap, fc, kEx_IllegalFunc); return; }
            handleFC16(client, mbap, startAddr, word2, &pdu[5]); // pdu[5]=byteCount, data starts at pdu[6]
            break;
        default:
            sendException(client, mbap, fc, kEx_IllegalFunc);
            break;
    }
}

// ── FC01 — Read Coils (from cache) ────────────────────────────────────────
void ModbusTcpService::handleFC01(WiFiClient& client, const uint8_t* mbap,
                                   uint16_t startAddr, uint16_t qty) {
    using namespace ModbusRegisterMap;
    if (qty == 0 || qty > kCoil_Count ||
        startAddr < kCoil_Base || startAddr + qty - 1 >= kCoil_Base + kCoil_Count) {
        sendException(client, mbap, kFC_ReadCoils, kEx_IllegalAddr);
        return;
    }

    const uint8_t byteCount = static_cast<uint8_t>((qty + 7) / 8);
    uint8_t resp[9 + 4] = {0};
    setMbap(resp, mbap, static_cast<uint16_t>(1 + 1 + 1 + byteCount));
    resp[7] = kFC_ReadCoils;
    resp[8] = byteCount;
    for (uint16_t i = 0; i < qty; ++i) {
        if (registerCache_.readCoil(static_cast<uint16_t>(startAddr + i - kCoil_Base))) {
            resp[9 + i / 8] |= static_cast<uint8_t>(1 << (i % 8));
        }
    }
    client.write(resp, static_cast<size_t>(9 + byteCount));
}

// ── FC02 — Read Discrete Inputs (from cache) ──────────────────────────────
void ModbusTcpService::handleFC02(WiFiClient& client, const uint8_t* mbap,
                                   uint16_t startAddr, uint16_t qty) {
    using namespace ModbusRegisterMap;
    if (qty == 0 || qty > kDI_Count ||
        startAddr < kDI_Base || startAddr + qty - 1 >= kDI_Base + kDI_Count) {
        sendException(client, mbap, kFC_ReadDI, kEx_IllegalAddr);
        return;
    }

    const uint8_t byteCount = static_cast<uint8_t>((qty + 7) / 8);
    uint8_t resp[9 + 1] = {0};
    setMbap(resp, mbap, static_cast<uint16_t>(1 + 1 + 1 + byteCount));
    resp[7] = kFC_ReadDI;
    resp[8] = byteCount;
    for (uint16_t i = 0; i < qty; ++i) {
        if (registerCache_.readDI(static_cast<uint16_t>(startAddr + i - kDI_Base))) {
            resp[9 + i / 8] |= static_cast<uint8_t>(1 << (i % 8));
        }
    }
    client.write(resp, static_cast<size_t>(9 + byteCount));
}

// ── FC03/FC04 — Read Holding / Input Registers (from cache) ──────────────
void ModbusTcpService::handleFC03(WiFiClient& client, const uint8_t* mbap, uint8_t fc,
                                   uint16_t startAddr, uint16_t qty) {
    using namespace ModbusRegisterMap;
    if (qty == 0 || qty > 125 ||
        startAddr < kHR_Base || startAddr + qty - 1 >= kHR_Base + kHR_Count) {
        sendException(client, mbap, fc, kEx_IllegalAddr);
        return;
    }

    // Response: 7(MBAP) + 1(FC) + 1(byteCount) + qty*2
    uint8_t resp[9 + 125 * 2] = {0};
    setMbap(resp, mbap, static_cast<uint16_t>(1 + 1 + 1 + qty * 2));
    resp[7] = fc;
    resp[8] = static_cast<uint8_t>(qty * 2);
    registerCache_.readBlock(static_cast<uint16_t>(startAddr - kHR_Base), qty, &resp[9]);
    client.write(resp, static_cast<size_t>(9 + qty * 2));
}

// ── FC05 — Write Single Coil ──────────────────────────────────────────────
void ModbusTcpService::handleFC05(WiFiClient& client, const uint8_t* mbap,
                                   uint16_t coilAddr, uint16_t coilValue) {
    using namespace ModbusRegisterMap;
    if (coilValue != 0x0000 && coilValue != 0xFF00) {
        sendException(client, mbap, kFC_WriteCoil, kEx_IllegalValue);
        return;
    }
    if (coilAddr < kCoil_Base || coilAddr >= kCoil_Base + kCoil_Count) {
        sendException(client, mbap, kFC_WriteCoil, kEx_IllegalAddr);
        return;
    }
    if (!writeCoil(static_cast<uint16_t>(coilAddr - kCoil_Base),
                   coilValue == 0xFF00, statusStore_)) {
        sendException(client, mbap, kFC_WriteCoil, kEx_IllegalAddr);
        return;
    }

    // Echo: MBAP(7) + FC(1) + coilAddr(2) + coilValue(2) = 12 bytes
    uint8_t resp[12] = {0};
    setMbap(resp, mbap, 6);  // UnitID(1)+FC(1)+addr(2)+val(2)=6
    resp[7] = kFC_WriteCoil;
    writeU16(&resp[8],  coilAddr);
    writeU16(&resp[10], coilValue);
    client.write(resp, 12);
}

// ── FC06 — Write Single Register ─────────────────────────────────────────
void ModbusTcpService::handleFC06(WiFiClient& client, const uint8_t* mbap,
                                   uint16_t regAddr, uint16_t regValue) {
    using namespace ModbusRegisterMap;
    if (regAddr < kHR_Base || regAddr >= kHR_Base + kHR_Count) {
        sendException(client, mbap, kFC_WriteHR, kEx_IllegalAddr);
        return;
    }
    const uint16_t pdAddr = static_cast<uint16_t>(regAddr - kHR_Base);
    // Route HMI block writes to HmiOperationService (always allowed in any build)
    if (pdAddr >= kHR_HmiBase && hmiService_) {
        if (!hmiService_->pendWrite(pdAddr - kHR_HmiBase, regValue)) {
            sendException(client, mbap, kFC_WriteHR, kEx_IllegalValue);
            return;
        }
    } else if (!writeHR(pdAddr, regValue, statusStore_, settingsStore_, fillController_, rtcService_)) {
        sendException(client, mbap, kFC_WriteHR, kEx_IllegalValue);
        return;
    }
    registerCache_.updateFast(statusStore_.snapshot(), settingsStore_, mqttConnected_);

    // Echo: same 12-byte pattern
    uint8_t resp[12] = {0};
    setMbap(resp, mbap, 6);
    resp[7] = kFC_WriteHR;
    writeU16(&resp[8],  regAddr);
    writeU16(&resp[10], regValue);
    client.write(resp, 12);
}

// ── FC16 — Write Multiple Registers ──────────────────────────────────────
void ModbusTcpService::handleFC16(WiFiClient& client, const uint8_t* mbap,
                                   uint16_t startAddr, uint16_t qty, const uint8_t* data) {
    using namespace ModbusRegisterMap;
    const uint8_t byteCount = data[0];
    if (qty == 0 || qty > 123 ||
        byteCount != qty * 2 ||
        startAddr < kHR_Base || startAddr + qty - 1 >= kHR_Base + kHR_Count) {
        sendException(client, mbap, kFC_WriteMultiHR,
                      (qty == 0 || qty > 123 || byteCount != qty * 2) ? kEx_IllegalValue : kEx_IllegalAddr);
        return;
    }

    for (uint16_t i = 0; i < qty; ++i) {
        const uint16_t addr = static_cast<uint16_t>(startAddr + i - kHR_Base);
        const uint16_t val  = readU16(&data[1 + i * 2]);
        // Route HMI block writes to HmiOperationService
        if (addr >= kHR_HmiBase && hmiService_) {
            if (!hmiService_->pendWrite(addr - kHR_HmiBase, val)) {
                sendException(client, mbap, kFC_WriteMultiHR, kEx_IllegalValue);
                return;
            }
        } else if (!writeHR(addr, val, statusStore_, settingsStore_, fillController_, rtcService_)) {
            sendException(client, mbap, kFC_WriteMultiHR, kEx_IllegalValue);
            return;
        }
    }
    registerCache_.updateFast(statusStore_.snapshot(), settingsStore_, mqttConnected_);

    // Response: MBAP(7) + FC(1) + startAddr(2) + qty(2) = 12 bytes
    uint8_t resp[12] = {0};
    setMbap(resp, mbap, 6);
    resp[7] = kFC_WriteMultiHR;
    writeU16(&resp[8],  startAddr);
    writeU16(&resp[10], qty);
    client.write(resp, 12);
}

// ── sendException ─────────────────────────────────────────────────────────
void ModbusTcpService::sendException(WiFiClient& client, const uint8_t* mbap,
                                      uint8_t fc, uint8_t exCode) {
    ResourceMonitor::instance().incrementTcpError();
    uint8_t resp[9] = {0};
    setMbap(resp, mbap, 3);  // UnitID(1) + error-FC(1) + exCode(1) = 3
    resp[7] = static_cast<uint8_t>(fc | 0x80);
    resp[8] = exCode;
    client.write(resp, 9);
}
