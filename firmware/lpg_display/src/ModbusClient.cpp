#include "ModbusClient.h"
#include "DisplayConfig.h"

// Modbus holding register base on KC868-A6 (matches ModbusRegisterMap.h kHR_Base)
static constexpr uint16_t kHR_Base        = 100;
static constexpr uint16_t kHR_State       = 0;   // offset from base
static constexpr uint16_t kHR_LiveWeight  = 1;
static constexpr uint16_t kHR_NetWeight   = 3;
static constexpr uint16_t kHR_TargetWeight = 5;
static constexpr uint16_t kHR_RatePerKg   = 7;
static constexpr uint16_t kHR_Coils       = 9;   // packed coil bits
static constexpr uint16_t kHR_ReadCount   = 10;

void ModbusClient::begin() {
  Serial2.begin(DisplayConfig::kRtuBaud, SERIAL_8N1,
                DisplayConfig::kRtuRxPin, DisplayConfig::kRtuTxPin);
  if (DisplayConfig::kRtuDePin != 255) {
    pinMode(DisplayConfig::kRtuDePin, OUTPUT);
    digitalWrite(DisplayConfig::kRtuDePin, LOW);
  }
  Serial.println("[MBUS] ModbusClient started");
}

void ModbusClient::poll() {
  if (millis() - lastPollMs_ < kPollIntervalMs) return;
  lastPollMs_ = millis();

  uint16_t regs[kHR_ReadCount] = {};
  if (!readHoldingRegisters(kHR_Base + kHR_State, kHR_ReadCount, regs)) {
    snap_.valid = false;
    return;
  }

  snap_.stateCode       = (uint8_t)regs[kHR_State];
  // Registers store float×1000 as two consecutive uint16 (hi/lo)
  uint32_t lw = ((uint32_t)regs[kHR_LiveWeight] << 16) | regs[kHR_LiveWeight + 1];
  uint32_t nw = ((uint32_t)regs[kHR_NetWeight]  << 16) | regs[kHR_NetWeight  + 1];
  uint32_t tw = ((uint32_t)regs[kHR_TargetWeight] << 16) | regs[kHR_TargetWeight + 1];
  uint32_t rk = ((uint32_t)regs[kHR_RatePerKg]  << 16) | regs[kHR_RatePerKg  + 1];
  snap_.liveWeightKg    = (float)lw / 1000.0f;
  snap_.netWeightKg     = (float)nw / 1000.0f;
  snap_.targetWeightKg  = (float)tw / 1000.0f;
  snap_.ratePerKg       = (float)rk / 100.0f;
  uint16_t coils        = regs[kHR_Coils];
  snap_.nozzleEngaged   = (coils >> 0) & 1;
  snap_.cylinderPresent = (coils >> 1) & 1;
  snap_.eStopOk         = (coils >> 2) & 1;
  snap_.valid = true;
}

bool ModbusClient::readHoldingRegisters(uint16_t startReg, uint16_t count, uint16_t* out) {
  uint8_t req[8];
  req[0] = DisplayConfig::kRtuAddr;
  req[1] = 0x03;
  req[2] = (uint8_t)(startReg >> 8);
  req[3] = (uint8_t)(startReg & 0xFF);
  req[4] = (uint8_t)(count >> 8);
  req[5] = (uint8_t)(count & 0xFF);
  uint16_t crc = crc16(req, 6);
  req[6] = (uint8_t)(crc & 0xFF);
  req[7] = (uint8_t)(crc >> 8);

  if (DisplayConfig::kRtuDePin != 255) {
    digitalWrite(DisplayConfig::kRtuDePin, HIGH);
    delayMicroseconds(50);
  }
  Serial2.write(req, 8);
  Serial2.flush();
  if (DisplayConfig::kRtuDePin != 255) {
    delayMicroseconds(50);
    digitalWrite(DisplayConfig::kRtuDePin, LOW);
  }

  const uint16_t expectedLen = 5 + count * 2;
  uint8_t resp[256];
  unsigned long t0 = millis();
  uint16_t rxLen = 0;
  while (millis() - t0 < 150 && rxLen < expectedLen) {
    if (Serial2.available()) resp[rxLen++] = (uint8_t)Serial2.read();
  }
  if (rxLen < expectedLen) return false;

  uint16_t rxCrc   = (uint16_t)resp[rxLen-2] | ((uint16_t)resp[rxLen-1] << 8);
  uint16_t calcCrc = crc16(resp, rxLen - 2);
  if (rxCrc != calcCrc) return false;

  for (uint16_t i = 0; i < count; i++) {
    out[i] = ((uint16_t)resp[3 + i*2] << 8) | resp[4 + i*2];
  }
  return true;
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
