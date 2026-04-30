#include "ModbusTcpService.h"

#include "ModbusRegisterMap.h"

namespace {
constexpr uint8_t kFunctionReadHolding = 0x03;
constexpr uint8_t kFunctionWriteSingle = 0x06;
constexpr uint8_t kExceptionIllegalFunction = 0x01;
constexpr uint8_t kExceptionIllegalAddress = 0x02;
constexpr uint8_t kExceptionIllegalValue = 0x03;

uint16_t readU16(const uint8_t* data) {
  return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

void writeU16(uint8_t* data, uint16_t value) {
  data[0] = static_cast<uint8_t>(value >> 8);
  data[1] = static_cast<uint8_t>(value & 0xff);
}

bool supportedRegister(uint16_t address) {
  return address >= ModbusRegisterMap::kLiveWeight && address <= ModbusRegisterMap::kEstopStatus;
}
}  // namespace

ModbusTcpService::ModbusTcpService(StatusStore& statusStore) : statusStore_(statusStore) {}

void ModbusTcpService::begin() {
  server_.begin();
  server_.setNoDelay(true);
  Serial.println(F("[MODBUS] TCP server started on port 502"));
}

void ModbusTcpService::handleClient() {
  WiFiClient client = server_.available();
  if (!client) {
    return;
  }

  const unsigned long startedAt = millis();
  while (client.connected() && client.available() < 12 && millis() - startedAt < 20) {
    delay(1);
  }

  uint8_t request[64] = {0};
  const int length = client.read(request, sizeof(request));
  if (length >= 12) {
    handleRequest(client, request, static_cast<uint16_t>(length));
  }
  client.stop();
}

void ModbusTcpService::handleRequest(WiFiClient& client, const uint8_t* request, uint16_t length) {
  const uint8_t unitId = request[6];
  const uint8_t functionCode = request[7];
  const uint16_t address = readU16(&request[8]);
  const uint16_t value = readU16(&request[10]);

  if (functionCode == kFunctionReadHolding) {
    const uint16_t quantity = value;
    if (quantity == 0 || quantity > ModbusRegisterMap::kRegisterCount || !supportedRegister(address) ||
        !supportedRegister(address + quantity - 1)) {
      sendException(client, request, functionCode, kExceptionIllegalAddress);
      return;
    }

    uint8_t response[32] = {0};
    memcpy(response, request, 4);
    writeU16(&response[4], static_cast<uint16_t>(3 + quantity * 2));
    response[6] = unitId;
    response[7] = functionCode;
    response[8] = static_cast<uint8_t>(quantity * 2);

    const StatusSnapshot status = statusStore_.snapshot();
    for (uint16_t i = 0; i < quantity; ++i) {
      writeU16(&response[9 + i * 2], ModbusRegisterMap::readHoldingRegister(address + i, status));
    }

    client.write(response, static_cast<size_t>(9 + quantity * 2));
    return;
  }

  if (functionCode == kFunctionWriteSingle) {
    if (!supportedRegister(address)) {
      sendException(client, request, functionCode, kExceptionIllegalAddress);
      return;
    }
    if (!ModbusRegisterMap::writeHoldingRegister(address, value, statusStore_)) {
      sendException(client, request, functionCode, kExceptionIllegalValue);
      return;
    }

    client.write(request, 12);
    return;
  }

  sendException(client, request, functionCode, kExceptionIllegalFunction);
}

void ModbusTcpService::sendException(WiFiClient& client, const uint8_t* request, uint8_t functionCode,
                                     uint8_t exceptionCode) {
  uint8_t response[9] = {0};
  memcpy(response, request, 4);
  writeU16(&response[4], 3);
  response[6] = request[6];
  response[7] = functionCode | 0x80;
  response[8] = exceptionCode;
  client.write(response, sizeof(response));
}
