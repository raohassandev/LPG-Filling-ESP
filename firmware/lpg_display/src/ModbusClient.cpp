#include "ModbusClient.h"
#include "DisplayConfig.h"
#include <string.h>

// FLOAT32 decode: two uint16 big-endian (Hi word at lower address) → IEEE-754
float ModbusClient::regsToFloat(uint16_t hi, uint16_t lo) {
  uint32_t bits = ((uint32_t)hi << 16) | lo;
  float f;
  memcpy(&f, &bits, sizeof(f));
  return f;
}

static HardwareSerial RtuSerial(DisplayConfig::kRtuUartNum);

void ModbusClient::begin() {
  RtuSerial.begin(DisplayConfig::kRtuBaud, SERIAL_8N1,
                  DisplayConfig::kRtuRxPin, DisplayConfig::kRtuTxPin);
  if (DisplayConfig::kRtuDePin != 255) {
    pinMode(DisplayConfig::kRtuDePin, OUTPUT);
    digitalWrite(DisplayConfig::kRtuDePin, LOW);
  }
  Serial.println("[MBUS] Modbus RTU master started");
}

void ModbusClient::poll() {
  const unsigned long now = millis();

  // Fast poll: weights, state, flags (registers 0x0000–0x0017, 24 regs)
  if (now - lastFastMs_ >= kFastMs) {
    lastFastMs_ = now;
    uint16_t r[24] = {};
    if (readHR(0x0000, 24, r)) {
      snap_.liveWeightKg   = regsToFloat(r[0x00], r[0x01]);
      snap_.tareWeightKg   = regsToFloat(r[0x02], r[0x03]);
      snap_.netWeightKg    = regsToFloat(r[0x04], r[0x05]);
      snap_.targetWeightKg = regsToFloat(r[0x06], r[0x07]);
      snap_.ratePerKg      = regsToFloat(r[0x08], r[0x09]);
      snap_.targetAmount   = regsToFloat(r[0x0A], r[0x0B]);
      snap_.currentAmount  = regsToFloat(r[0x0C], r[0x0D]);
      snap_.state          = static_cast<FillState>(r[0x0E]);
      snap_.eStopOk        = r[0x0F] != 0;
      snap_.cylinderPresent= r[0x10] != 0;
      snap_.nozzleEngaged  = r[0x11] != 0;
      snap_.weightStable   = r[0x12] != 0;
      snap_.valid          = true;
      snap_.connected      = true;
      snap_.lastOkMs       = now;
    } else {
      if (now - snap_.lastOkMs > 3000) snap_.connected = false;
    }
  }

  // Slow poll: RTC (registers 0x0020–0x0025, 6 regs)
  if (now - lastSlowMs_ >= kSlowMs) {
    lastSlowMs_ = now;
    uint16_t r[6] = {};
    if (readHR(0x0020, 6, r)) {
      snap_.rtcHour   = r[3];
      snap_.rtcMinute = r[4];
      snap_.rtcSecond = r[5];
    }
  }

  // Stats poll: today stats (registers 0x0030–0x0035, 6 regs)
  if (now - lastStatMs_ >= kStatMs) {
    lastStatMs_ = now;
    uint16_t r[6] = {};
    if (readHR(0x0030, 6, r)) {
      snap_.todayFills  = r[0];
      snap_.todayFails  = r[1];
      snap_.todayKg     = regsToFloat(r[2], r[3]);
      snap_.todayAmount = regsToFloat(r[4], r[5]);
    }
  }
}

bool ModbusClient::writeRegister(uint16_t regAddr, uint16_t value) {
  uint8_t req[8];
  req[0] = DisplayConfig::kRtuSlaveAddr;
  req[1] = 0x06;
  req[2] = (uint8_t)(regAddr >> 8);
  req[3] = (uint8_t)(regAddr & 0xFF);
  req[4] = (uint8_t)(value >> 8);
  req[5] = (uint8_t)(value & 0xFF);
  uint16_t crc = crc16(req, 6);
  req[6] = (uint8_t)(crc & 0xFF);
  req[7] = (uint8_t)(crc >> 8);

  uint8_t resp[8];
  return sendAndReceive(req, 8, resp, 8);
}

bool ModbusClient::readHR(uint16_t startReg, uint16_t count, uint16_t* out) {
  uint8_t req[8];
  req[0] = DisplayConfig::kRtuSlaveAddr;
  req[1] = 0x03;
  req[2] = (uint8_t)(startReg >> 8);
  req[3] = (uint8_t)(startReg & 0xFF);
  req[4] = (uint8_t)(count >> 8);
  req[5] = (uint8_t)(count & 0xFF);
  uint16_t crc = crc16(req, 6);
  req[6] = (uint8_t)(crc & 0xFF);
  req[7] = (uint8_t)(crc >> 8);

  const uint16_t expectedLen = 5 + count * 2;
  uint8_t resp[256];
  if (!sendAndReceive(req, 8, resp, expectedLen)) return false;

  for (uint16_t i = 0; i < count; i++)
    out[i] = ((uint16_t)resp[3 + i * 2] << 8) | resp[4 + i * 2];
  return true;
}

bool ModbusClient::sendAndReceive(uint8_t* req, uint8_t reqLen,
                                   uint8_t* resp, uint16_t expectedLen) {
  // Flush stale bytes
  while (RtuSerial.available()) RtuSerial.read();

  if (DisplayConfig::kRtuDePin != 255) {
    digitalWrite(DisplayConfig::kRtuDePin, HIGH);
    delayMicroseconds(100);
  }
  RtuSerial.write(req, reqLen);
  RtuSerial.flush();
  if (DisplayConfig::kRtuDePin != 255) {
    delayMicroseconds(100);
    digitalWrite(DisplayConfig::kRtuDePin, LOW);
  }

  unsigned long t0 = millis();
  uint16_t rxLen = 0;
  while (millis() - t0 < kTimeoutMs && rxLen < expectedLen) {
    if (RtuSerial.available()) resp[rxLen++] = (uint8_t)RtuSerial.read();
  }
  if (rxLen < expectedLen) return false;

  uint16_t rxCrc   = (uint16_t)resp[rxLen - 2] | ((uint16_t)resp[rxLen - 1] << 8);
  uint16_t calcCrc = crc16(resp, rxLen - 2);
  return rxCrc == calcCrc;
}

uint16_t ModbusClient::crc16(const uint8_t* data, uint16_t len) {
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
