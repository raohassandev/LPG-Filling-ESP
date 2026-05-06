#pragma once
#include <Arduino.h>

// Fill-state codes (matches kFillState_* in ModbusRegisterMap.h)
enum class FillState : uint8_t {
  Idle        = 0,
  Ready       = 1,
  Validating  = 2,
  Fast        = 3,
  Slow        = 4,
  Settling    = 5,
  Complete    = 6,
  Aborted     = 7,
  Fault       = 8,
  Maintenance = 9,
};

// Full controller snapshot — populated from Modbus RTU reads
struct ControllerSnapshot {
  // Weights
  float liveWeightKg    = 0.0f;
  float tareWeightKg    = 0.0f;
  float netWeightKg     = 0.0f;
  float targetWeightKg  = 0.0f;
  float ratePerKg       = 0.0f;
  float targetAmount    = 0.0f;
  float currentAmount   = 0.0f;

  // State & flags
  FillState state       = FillState::Idle;
  bool eStopOk          = false;
  bool cylinderPresent  = false;
  bool nozzleEngaged    = false;
  bool weightStable     = false;

  // Stats — today
  uint16_t todayFills   = 0;
  uint16_t todayFails   = 0;
  float    todayKg      = 0.0f;
  float    todayAmount  = 0.0f;

  // RTC
  uint16_t rtcHour      = 0;
  uint16_t rtcMinute    = 0;
  uint16_t rtcSecond    = 0;

  // Comms
  bool valid            = false;   // false until first successful poll
  bool connected        = false;
  unsigned long lastOkMs = 0;
};

class ModbusClient {
public:
  void begin();
  void poll();

  const ControllerSnapshot& snapshot() const { return snap_; }

  // Write FC06 single register
  bool writeRegister(uint16_t regAddr, uint16_t value);

  // Convenience commands
  bool cmdStart()   { return writeRegister(0x0017, 1); }
  bool cmdStop()    { return writeRegister(0x0017, 2); }
  bool cmdReset()   { return writeRegister(0x0017, 3); }
  bool cmdZeroNet() { return writeRegister(0x0017, 4); }

private:
  ControllerSnapshot snap_;

  unsigned long lastFastMs_ = 0;
  unsigned long lastSlowMs_ = 0;
  unsigned long lastStatMs_ = 0;

  static constexpr unsigned long kFastMs = 200;
  static constexpr unsigned long kSlowMs = 2000;
  static constexpr unsigned long kStatMs = 5000;
  static constexpr unsigned long kTimeoutMs = 150;

  bool readHR(uint16_t startReg, uint16_t count, uint16_t* out);
  bool sendAndReceive(uint8_t* req, uint8_t reqLen, uint8_t* resp, uint16_t expectedLen);
  uint16_t crc16(const uint8_t* data, uint16_t len);

  static float regsToFloat(uint16_t hi, uint16_t lo);
};
