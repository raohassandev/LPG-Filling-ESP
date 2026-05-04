#pragma once

#include <Arduino.h>

enum class ProcessState : uint8_t {
  Boot,
  Idle,
  Ready,
  Validating,
  FillingFast,
  FillingSlow,
  Settling,
  Complete,
  Aborted,
  Fault,
  Maintenance,
};

struct StatusSnapshot {
  ProcessState state = ProcessState::Boot;
  String stateLabel = "BOOT";
  String bootReason = "unknown";
  float liveWeightKg = 0.0f;
  float tareWeightKg = 0.0f;
  float netWeightKg = 0.0f;
  float targetWeightKg = 0.0f;
  float targetAmount = 0.0f;
  float ratePerKg = 0.0f;
  bool relays[6] = {false, false, false, false, false, false};
  bool inputs[6] = {false, false, false, false, false, false};
  bool nozzleEngaged = false;
  bool cylinderPresent = false;
  bool emergencyStopOk = true;
  bool weightStable = false;
  String lastReasonCode;
  unsigned long uptimeMs = 0;
};

class StatusStore {
 public:
  void begin();
  void setBootReason(const String& value);
  void setState(ProcessState state, const String& label);
  void setWeight(float kg);
  void setTareWeight(float kg);
  void setTargets(float targetKg, float targetAmount, float ratePerKg);
  void setRelay(uint8_t index, bool active);
  void setInput(uint8_t index, bool active);
  void setNozzleEngaged(bool engaged);
  void setCylinderPresent(bool present);
  void setEmergencyStopOk(bool ok);
  void setWeightStable(bool stable);
  void setReasonCode(const String& code);
  StatusSnapshot snapshot() const;

 private:
  StatusSnapshot status_;
};
