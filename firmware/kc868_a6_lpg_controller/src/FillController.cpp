#include "FillController.h"

FillController::FillController(StatusStore& statusStore, RelayBank& relayBank, InputExpander& inputExpander,
                               WeightService& weightService, SettingsStore& settingsStore, EventLog& eventLog)
    : statusStore_(statusStore),
      relayBank_(relayBank),
      inputExpander_(inputExpander),
      weightService_(weightService),
      settingsStore_(settingsStore),
      eventLog_(eventLog) {}

void FillController::begin() {
  relayBank_.writeAllSafe();
  syncInputs();
  transitionTo(ProcessState::Idle, "IDLE");
  eventLog_.append("INFO", "boot", "Controller entered idle state");
}

void FillController::tick() {
  syncInputs();
  statusStore_.setWeight(weightService_.liveWeightKg());

  const StatusSnapshot status = statusStore_.snapshot();
  if (!status.emergencyStopOk && status.state != ProcessState::Fault) {
    setFault("emergency_stop");
    return;
  }

  if (isActiveFillState(status.state) && !status.nozzleEngaged) {
    setFault("nozzle_disengaged");
    return;
  }

  const float slowFillThreshold = settingsStore_.snapshot().slowFillThreshold;
  if (status.state == ProcessState::FillingFast && status.liveWeightKg >= status.targetWeightKg * slowFillThreshold) {
    relayBank_.writeRelay(1, false);
    relayBank_.writeRelay(2, true);
    syncRelays();
    transitionTo(ProcessState::FillingSlow, "FILLING_SLOW");
    eventLog_.append("INFO", "fill_slow", "Controller entered slow fill");
    return;
  }

  if (status.state == ProcessState::FillingSlow && status.liveWeightKg >= status.targetWeightKg) {
    relayBank_.writeAllSafe();
    syncRelays();
    transitionTo(ProcessState::Complete, "COMPLETE");
    eventLog_.append("INFO", "fill_complete", "Target reached and outputs de-energized");
  }
}

bool FillController::startFill(float targetWeightKg, float ratePerKg, float targetAmount, String& reason) {
  syncInputs();
  const StatusSnapshot status = statusStore_.snapshot();

  if (isActiveFillState(status.state)) {
    reason = "Fill is already active";
    return false;
  }

  if (status.state == ProcessState::Fault) {
    reason = "Reset fault before starting";
    return false;
  }

  if (status.state == ProcessState::Maintenance) {
    reason = "Maintenance operation is in progress";
    return false;
  }

  const bool startStateAllowed = status.state == ProcessState::Idle || status.state == ProcessState::Ready ||
                                 status.state == ProcessState::Complete || status.state == ProcessState::Aborted;
  if (!startStateAllowed) {
    reason = "Controller state does not allow filling";
    return false;
  }

  if (!status.emergencyStopOk) {
    reason = "Emergency stop is active";
    return false;
  }

  if (!status.cylinderPresent) {
    reason = "Cylinder is not detected";
    return false;
  }

  if (!status.nozzleEngaged) {
    reason = "Nozzle is not engaged";
    return false;
  }

  if (targetWeightKg <= 0.0f || ratePerKg <= 0.0f) {
    reason = "Target weight and rate must be positive";
    return false;
  }

  statusStore_.setTargets(targetWeightKg, targetAmount, ratePerKg);
  relayBank_.writeAllSafe();
  relayBank_.writeRelay(0, true);
  relayBank_.writeRelay(1, true);
  syncRelays();
  transitionTo(ProcessState::FillingFast, "FILLING_FAST");
  eventLog_.append("INFO", "fill_start", "Fill started");
  reason = "started";
  return true;
}

bool FillController::stopFill(const String& reasonCode) {
  const StatusSnapshot status = statusStore_.snapshot();
  if (!isActiveFillState(status.state) && status.state != ProcessState::Complete) {
    return false;
  }

  relayBank_.writeAllSafe();
  syncRelays();
  transitionTo(ProcessState::Aborted, "ABORTED", reasonCode);
  eventLog_.append("WARN", reasonCode, "Fill stopped by operator or serial command");
  return true;
}

bool FillController::resetToIdle(String& reason) {
  syncInputs();
  const StatusSnapshot status = statusStore_.snapshot();

  if (status.state == ProcessState::Maintenance) {
    reason = "Maintenance operation is in progress";
    return false;
  }

  if (!status.emergencyStopOk) {
    reason = "Emergency stop is active";
    return false;
  }

  relayBank_.writeAllSafe();
  syncRelays();
  statusStore_.setTargets(0.0f, 0.0f, 0.0f);
  transitionTo(ProcessState::Idle, "IDLE");
  eventLog_.append("INFO", "reset", "Controller returned to idle");
  reason = "idle";
  return true;
}

void FillController::syncInputs() {
  const bool cylinderPresent = inputExpander_.inputState(0);
  const bool nozzleEngaged = inputExpander_.inputState(1);
  const bool emergencyOk = !inputExpander_.inputState(3);

  statusStore_.setCylinderPresent(cylinderPresent);
  statusStore_.setNozzleEngaged(nozzleEngaged);
  statusStore_.setEmergencyStopOk(emergencyOk);

  for (uint8_t i = 0; i < 6; ++i) {
    statusStore_.setInput(i, inputExpander_.inputState(i));
  }
}

void FillController::syncRelays() {
  for (uint8_t i = 0; i < 6; ++i) {
    statusStore_.setRelay(i, relayBank_.relayState(i));
  }
}

void FillController::transitionTo(ProcessState state, const String& label, const String& reasonCode) {
  stateStartedMs_ = millis();
  statusStore_.setState(state, label);
  statusStore_.setReasonCode(reasonCode);
}

void FillController::setFault(const String& reasonCode) {
  relayBank_.writeAllSafe();
  syncRelays();
  transitionTo(ProcessState::Fault, "FAULT", reasonCode);
  eventLog_.append("ERROR", reasonCode, "Safety fault forced outputs to safe state");
}

bool FillController::isActiveFillState(ProcessState state) const {
  return state == ProcessState::FillingFast || state == ProcessState::FillingSlow ||
         state == ProcessState::Settling;
}
