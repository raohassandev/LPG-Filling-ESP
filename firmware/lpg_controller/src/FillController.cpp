#include "FillController.h"

FillController::FillController(StatusStore& statusStore, RelayBank& relayBank, InputExpander& inputExpander,
                               WeightService& weightService, SettingsStore& settingsStore, EventLog& eventLog,
                               TransactionLog& transactionLog)
    : statusStore_(statusStore),
      relayBank_(relayBank),
      inputExpander_(inputExpander),
      weightService_(weightService),
      settingsStore_(settingsStore),
      eventLog_(eventLog),
      transactionLog_(transactionLog) {}

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
  const unsigned long nowMs = millis();

  // ── E-stop: fault immediately in any active state ─────────────────────────
  if (!status.emergencyStopOk && status.state != ProcessState::Fault) {
    setFault("emergency_stop");
    return;
  }

  if (!isActiveFillState(status.state)) {
    // ── Settling: close valves, wait for scale to stabilise, then complete ──
    if (status.state == ProcessState::Settling) {
      if (nowMs - stateStartedMs_ >= kSettlingMs && status.weightStable) {
        const StatusSnapshot s2 = statusStore_.snapshot();
        if (activeTransactionId_ != 0) {
          transactionLog_.completeTransaction(activeTransactionId_, s2.liveWeightKg, s2.netWeightKg);
          activeTransactionId_ = 0;
        }
        transitionTo(ProcessState::Complete, "COMPLETE");
        eventLog_.append("INFO", "fill_complete", "Settling complete — fill transaction closed");
      }
    }
    return;
  }

  // ── Nozzle disengaged during active fill ──────────────────────────────────
  if (!status.nozzleEngaged) {
    setFault("nozzle_disengaged");
    return;
  }

  // ── Scale read failure during fill ───────────────────────────────────────
  if (weightService_.readFailed()) {
    setFault("scale_read_error");
    return;
  }

  // ── Overfill guard: target + 500 g hard stop ─────────────────────────────
  if (status.netWeightKg >= status.targetWeightKg + kOverfillMarginKg) {
    setFault("overfill");
    return;
  }

  // ── Max fill timeout ──────────────────────────────────────────────────────
  if (nowMs - stateStartedMs_ >= kMaxFillMs) {
    setFault("fill_timeout");
    return;
  }

  // ── No-flow detection: sample weight every kNoFlowWindowMs ───────────────
  if (nowMs - noFlowWindowStartMs_ >= kNoFlowWindowMs) {
    const float delta = status.netWeightKg - noFlowWindowStartKg_;
    if (delta < kNoFlowMinDeltaKg) {
      setFault("no_flow");
      return;
    }
    noFlowWindowStartMs_ = nowMs;
    noFlowWindowStartKg_ = status.netWeightKg;
  }

  // ── Fast→Slow transition ──────────────────────────────────────────────────
  const float slowFillThreshold = settingsStore_.snapshot().slowFillThreshold;
  if (status.state == ProcessState::FillingFast &&
      status.netWeightKg >= status.targetWeightKg * slowFillThreshold) {
    relayBank_.writeRelay(0, false);
    relayBank_.writeRelay(1, true);
    relayBank_.writeRelay(2, true);
    syncRelays();
    // Reset no-flow window for slow fill phase
    noFlowWindowStartMs_ = nowMs;
    noFlowWindowStartKg_ = status.netWeightKg;
    transitionTo(ProcessState::FillingSlow, "FILLING_SLOW");
    eventLog_.append("INFO", "fill_slow", "Switched to slow fill");
    return;
  }

  // ── Target reached → enter Settling ──────────────────────────────────────
  if (status.state == ProcessState::FillingSlow && status.netWeightKg >= status.targetWeightKg) {
    relayBank_.writeAllSafe();
    syncRelays();
    transitionTo(ProcessState::Settling, "SETTLING");
    eventLog_.append("INFO", "fill_settling", "Target reached — entering settling");
  }
}

bool FillController::startFill(float targetWeightKg, float ratePerKg, float targetAmount, String& reason,
                               const String& operatorUsername) {
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

  // Scale health checks — must be initialized, reading, stable, and calibrated before fill
  if (!weightService_.initialized()) {
    reason = "Scale not initialized — check HX711 wiring";
    return false;
  }
  if (weightService_.readFailed()) {
    reason = "Scale read error — check HX711 connection";
    return false;
  }
  if (!weightService_.stable()) {
    reason = "Scale not stable — wait for weight to settle";
    return false;
  }
  if (!weightService_.calibrationValid()) {
    reason = "Scale not calibrated — calibrate before filling";
    return false;
  }

  // Block fill if transaction log cannot create a record (e.g. SPIFFS full)
  activeTransactionId_ =
      transactionLog_.startTransaction(targetWeightKg, ratePerKg, targetAmount, status.tareWeightKg, "api", operatorUsername);
  if (activeTransactionId_ == 0) {
    reason = "Transaction log failed — check storage";
    return false;
  }

  noFlowWindowStartMs_ = millis();
  noFlowWindowStartKg_ = status.netWeightKg;

  statusStore_.setTargets(targetWeightKg, targetAmount, ratePerKg);
  relayBank_.writeAllSafe();
  relayBank_.writeRelay(0, true);
  relayBank_.writeRelay(2, true);
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
  if (activeTransactionId_ != 0) {
    transactionLog_.abortTransaction(activeTransactionId_, reasonCode);
    activeTransactionId_ = 0;
  }
  transitionTo(ProcessState::Aborted, "ABORTED", reasonCode);
  eventLog_.append("WARN", reasonCode, "Fill stopped by operator or serial command");
  return true;
}

bool FillController::resetToIdle(String& reason) {
  syncInputs();
  const StatusSnapshot status = statusStore_.snapshot();

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
  bool cylinderPresent, nozzleEngaged;
  if (simInputsActive_) {
    cylinderPresent = simCylinderPresent_;
    nozzleEngaged   = simNozzleEngaged_;
  } else {
    cylinderPresent = inputExpander_.inputState(BoardConfig::kInputCylinderPresent);
    nozzleEngaged   = inputExpander_.inputState(BoardConfig::kInputNozzleEngaged);
  }
  const bool rawEmergency = inputExpander_.inputState(BoardConfig::kInputEmergencyStop);
  const bool emergencyOk = BoardConfig::kInputEmergencyRawMeansTripped ? !rawEmergency : rawEmergency;

  statusStore_.setCylinderPresent(cylinderPresent);
  statusStore_.setNozzleEngaged(nozzleEngaged);
  statusStore_.setEmergencyStopOk(emergencyOk);

  for (uint8_t i = 0; i < 6; ++i) {
    statusStore_.setInput(i, inputExpander_.inputState(i));
  }
}

void FillController::setSimInputs(bool cylinderPresent, bool nozzleEngaged) {
  simInputsActive_    = true;
  simCylinderPresent_ = cylinderPresent;
  simNozzleEngaged_   = nozzleEngaged;
  Serial.printf("[CTRL] Sim inputs active: cylinder=%d nozzle=%d\n", cylinderPresent, nozzleEngaged);
}

void FillController::clearSimInputs() {
  simInputsActive_ = false;
  Serial.println("[CTRL] Sim inputs cleared — reading physical inputs");
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
  if (activeTransactionId_ != 0) {
    transactionLog_.faultTransaction(activeTransactionId_, reasonCode);
    activeTransactionId_ = 0;
  }
  transitionTo(ProcessState::Fault, "FAULT", reasonCode);
  eventLog_.append("ERROR", reasonCode, "Safety fault forced outputs to safe state");
}

bool FillController::isActiveFillState(ProcessState state) const {
  return state == ProcessState::FillingFast || state == ProcessState::FillingSlow ||
         state == ProcessState::Settling;
}
