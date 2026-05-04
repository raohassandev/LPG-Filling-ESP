#include "ModbusTcpService.h"
#include "ModbusRegisterMap.h"

namespace {

// Function codes
constexpr uint8_t kFcReadHolding    = 0x03;
constexpr uint8_t kFcWriteSingle    = 0x06;
constexpr uint8_t kFcWriteMultiple  = 0x10;

// Exception codes
constexpr uint8_t kExcIllegalFunc   = 0x01;
constexpr uint8_t kExcIllegalAddr   = 0x02;
constexpr uint8_t kExcIllegalValue  = 0x03;

uint16_t readU16(const uint8_t* p) {
    return (static_cast<uint16_t>(p[0]) << 8) | p[1];
}

void writeU16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v & 0xFF);
}

bool inRange(uint16_t addr) {
    return addr >= ModbusRegisterMap::kRegisterBase &&
           addr <  ModbusRegisterMap::kRegisterBase + ModbusRegisterMap::kRegisterCount;
}

}  // namespace

ModbusTcpService::ModbusTcpService(StatusStore& statusStore, SettingsStore& settingsStore,
                                    FillController& fillController, TransactionLog& transactionLog)
    : statusStore_(statusStore),
      settingsStore_(settingsStore),
      fillController_(fillController),
      transactionLog_(transactionLog) {}

void ModbusTcpService::begin() {
    server_.begin();
    server_.setNoDelay(true);
    Serial.println(F("[MODBUS] TCP server started on port 502"));
    Serial.printf("[MODBUS] Registers 0x%04X–0x%04X (%u regs, dec %u–%u)\n",
                  ModbusRegisterMap::kRegisterBase,
                  ModbusRegisterMap::kRegisterBase + ModbusRegisterMap::kRegisterCount - 1,
                  ModbusRegisterMap::kRegisterCount,
                  ModbusRegisterMap::kRegisterBase,
                  ModbusRegisterMap::kRegisterBase + ModbusRegisterMap::kRegisterCount - 1);
}

void ModbusTcpService::handleClient() {
    WiFiClient client = server_.available();
    if (!client) return;

    const unsigned long t0 = millis();
    while (client.connected() && client.available() < 8 && millis() - t0 < 50)
        delay(1);

    uint8_t req[260] = {0};
    const int len = client.read(req, sizeof(req));
    if (len >= 8)
        handleRequest(client, req, static_cast<uint16_t>(len));

    client.stop();
}

void ModbusTcpService::handleRequest(WiFiClient& client, const uint8_t* req, uint16_t len) {
    // MBAP header: TransactionID(2) ProtocolID(2) Length(2) UnitID(1) FunctionCode(1)
    const uint8_t  fc      = req[7];
    const uint16_t startAddr = readU16(&req[8]);
    const uint16_t word2     = readU16(&req[10]);  // quantity (FC03) / value (FC06) / quantity (FC16)

    // ── FC03: Read Holding Registers ─────────────────────────────────────────
    if (fc == kFcReadHolding) {
        const uint16_t qty = word2;
        if (qty == 0 || qty > ModbusRegisterMap::kRegisterCount ||
            !inRange(startAddr) || !inRange(startAddr + qty - 1)) {
            sendException(client, req, fc, kExcIllegalAddr);
            return;
        }

        const StatusSnapshot status = statusStore_.snapshot();
        uint8_t resp[9 + 2 * 125] = {0};
        memcpy(resp, req, 4);                                     // echo TID + PID
        writeU16(&resp[4], static_cast<uint16_t>(3 + qty * 2));  // PDU length
        resp[6] = req[6];                                         // unit ID
        resp[7] = fc;
        resp[8] = static_cast<uint8_t>(qty * 2);                 // byte count

        for (uint16_t i = 0; i < qty; ++i) {
            uint16_t val = ModbusRegisterMap::readHoldingRegister(startAddr + i, status, transactionLog_);
            writeU16(&resp[9 + i * 2], val);
        }

        client.write(resp, static_cast<size_t>(9 + qty * 2));
        return;
    }

    // ── FC06: Write Single Register ──────────────────────────────────────────
    if (fc == kFcWriteSingle) {
        if (!inRange(startAddr)) {
            sendException(client, req, fc, kExcIllegalAddr);
            return;
        }
        if (!ModbusRegisterMap::writeHoldingRegister(startAddr, word2,
                                                      statusStore_, settingsStore_, fillController_)) {
            sendException(client, req, fc, kExcIllegalValue);
            return;
        }
        client.write(req, 12);  // echo request
        return;
    }

    // ── FC16: Write Multiple Registers ───────────────────────────────────────
    if (fc == kFcWriteMultiple) {
        const uint16_t qty  = word2;
        // byte count at req[12], data starts at req[13]
        if (len < static_cast<uint16_t>(13 + qty * 2) ||
            qty == 0 || qty > ModbusRegisterMap::kRegisterCount ||
            !inRange(startAddr) || !inRange(startAddr + qty - 1)) {
            sendException(client, req, fc, kExcIllegalAddr);
            return;
        }

        for (uint16_t i = 0; i < qty; ++i) {
            const uint16_t addr = startAddr + i;
            const uint16_t val  = readU16(&req[13 + i * 2]);
            // silently skip read-only registers so a bulk write doesn't abort
            ModbusRegisterMap::writeHoldingRegister(addr, val,
                                                     statusStore_, settingsStore_, fillController_);
        }

        // FC16 response: echo MBAP + fc + startAddr(2) + qty(2)
        uint8_t resp[12] = {0};
        memcpy(resp, req, 4);
        writeU16(&resp[4], 6);
        resp[6] = req[6];
        resp[7] = fc;
        writeU16(&resp[8],  startAddr);
        writeU16(&resp[10], qty);
        client.write(resp, 12);
        return;
    }

    sendException(client, req, fc, kExcIllegalFunc);
}

void ModbusTcpService::sendException(WiFiClient& client, const uint8_t* req,
                                      uint8_t fc, uint8_t exCode) {
    uint8_t resp[9] = {0};
    memcpy(resp, req, 4);
    writeU16(&resp[4], 3);
    resp[6] = req[6];
    resp[7] = fc | 0x80;
    resp[8] = exCode;
    client.write(resp, 9);
}
