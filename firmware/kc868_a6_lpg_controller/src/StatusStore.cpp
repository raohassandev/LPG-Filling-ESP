#include "StatusStore.h"

void StatusStore::begin() {
  status_.state = ProcessState::Boot;
  status_.stateLabel = "BOOT";
  status_.uptimeMs = millis();
}

void StatusStore::setBootReason(const String& value) { status_.bootReason = value; }

void StatusStore::setState(ProcessState state, const String& label) {
  status_.state = state;
  status_.stateLabel = label;
  status_.uptimeMs = millis();
}

void StatusStore::setWeight(float kg) {
  status_.liveWeightKg = kg;
  status_.netWeightKg = kg - status_.tareWeightKg;
  if (status_.netWeightKg < 0.0f) {
    status_.netWeightKg = 0.0f;
  }
}

void StatusStore::setTareWeight(float kg) {
  status_.tareWeightKg = kg < 0.0f ? 0.0f : kg;
  setWeight(status_.liveWeightKg);
}

void StatusStore::setTargets(float targetKg, float targetAmount, float ratePerKg) {
  status_.targetWeightKg = targetKg;
  status_.targetAmount = targetAmount;
  status_.ratePerKg = ratePerKg;
}

void StatusStore::setRelay(uint8_t index, bool active) {
  if (index >= 6) {
    return;
  }
  status_.relays[index] = active;
}

void StatusStore::setInput(uint8_t index, bool active) {
  if (index >= 6) {
    return;
  }
  status_.inputs[index] = active;
}

void StatusStore::setNozzleEngaged(bool engaged) { status_.nozzleEngaged = engaged; }

void StatusStore::setCylinderPresent(bool present) { status_.cylinderPresent = present; }

void StatusStore::setEmergencyStopOk(bool ok) { status_.emergencyStopOk = ok; }

void StatusStore::setWeightStable(bool stable) { status_.weightStable = stable; }

void StatusStore::setReasonCode(const String& code) { status_.lastReasonCode = code; }

StatusSnapshot StatusStore::snapshot() const {
  StatusSnapshot copy = status_;
  copy.uptimeMs = millis();
  return copy;
}
