#pragma once
#include <Arduino.h>

// Snapshot of controller state read via Modbus RTU FC03
struct ControllerSnapshot {
  float   liveWeightKg   = 0.0f;
  float   netWeightKg    = 0.0f;
  float   targetWeightKg = 0.0f;
  float   ratePerKg      = 0.0f;
  uint8_t stateCode      = 0;     // matches ProcessState enum on KC868-A6
  bool    nozzleEngaged  = false;
  bool    cylinderPresent = false;
  bool    eStopOk        = false;
  bool    valid          = false; // false until first successful read
};

class ModbusClient {
public:
  void begin();
  void poll();
  const ControllerSnapshot& snapshot() const { return snap_; }

private:
  ControllerSnapshot snap_;
  unsigned long      lastPollMs_ = 0;
  static constexpr unsigned long kPollIntervalMs = 200;

  bool readHoldingRegisters(uint16_t startReg, uint16_t count, uint16_t* out);
  uint16_t crc16(const uint8_t* data, uint16_t len);
};
