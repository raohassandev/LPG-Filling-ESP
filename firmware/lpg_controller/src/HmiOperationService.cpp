#pragma GCC optimize("Os")
#include "HmiOperationService.h"
#include "ModbusRegisterMap.h"

using namespace ModbusRegisterMap;

// ── float helpers ─────────────────────────────────────────────────────────────

float HmiOperationService::regsToFloat(uint16_t hi, uint16_t lo) {
    const uint32_t bits = (static_cast<uint32_t>(hi) << 16) | lo;
    float f; memcpy(&f, &bits, sizeof(f));
    return f;
}

void HmiOperationService::floatToRegs(float f, uint16_t& hi, uint16_t& lo) {
    uint32_t bits; memcpy(&bits, &f, sizeof(bits));
    hi = static_cast<uint16_t>(bits >> 16);
    lo = static_cast<uint16_t>(bits & 0xFFFF);
}

// ── begin ─────────────────────────────────────────────────────────────────────

void HmiOperationService::begin() {
    lastHbMs_ = millis();
    Serial.println(F("[HMI] HmiOperationService ready"));
}

// ── pendWrite (fast path — called from Modbus write handler) ─────────────────

bool HmiOperationService::pendWrite(uint16_t hmiAddr, uint16_t value) {
    // hmiAddr is 0-based within HMI block (0x0080 subtracted by caller)
    switch (hmiAddr) {
        // Command interface
        case 0x00: commandCode_ = value; return true;           // HMI_CommandCode
        case 0x01:                                               // HMI_CommandSeq
            commandSeq_ = value;
            if (value != lastAcceptedSeq_) {
                pendingCmd_  = true;
                pendingCode_ = commandCode_;
                pendingSeq_  = value;
            }
            return true;
        // Fill mode
        case 0x06: fillMode_ = (value > 1) ? 0 : value; return true;
        // Heartbeat
        case 0x0C:
            hbCounter_++;
            lastHbMs_ = millis();
            hbAgeSec_ = 0;
            return true;
        case 0x0D: wdtTimeout_ = value; return true;
        // Preset float Hi words (staged)
        case 0x10: stageTareHi_   = value; return true;
        case 0x11: presetTare_    = regsToFloat(stageTareHi_,   value); presetValid_ = 0; return true;
        case 0x12: stageTargetHi_ = value; return true;
        case 0x13: presetTarget_  = regsToFloat(stageTargetHi_, value); presetValid_ = 0; return true;
        case 0x14: stageRateHi_   = value; return true;
        case 0x15: presetRate_    = regsToFloat(stageRateHi_,   value); presetValid_ = 0; return true;
        case 0x16: stageAmountHi_ = value; return true;
        case 0x17: presetAmount_  = regsToFloat(stageAmountHi_, value); presetValid_ = 0; return true;
        default:   return false;  // read-only or unknown HMI register
    }
}

// ── validatePreset ────────────────────────────────────────────────────────────

bool HmiOperationService::validatePreset(uint16_t& errCode) const {
    if (fillMode_ == 0) {
        // By-kg mode: target weight must be positive
        if (presetTarget_ <= 0.0f || presetTarget_ > 500.0f) {
            errCode = kHmiErr_InvalidPreset; return false;
        }
    } else {
        // By-amount mode: amount > 0, rate > 0
        if (presetAmount_ <= 0.0f || presetRate_ <= 0.0f || presetRate_ > 100000.0f) {
            errCode = kHmiErr_InvalidPreset; return false;
        }
    }
    if (presetTare_ < 0.0f || presetTare_ > 500.0f) {
        errCode = kHmiErr_InvalidPreset; return false;
    }
    errCode = kHmiErr_None;
    return true;
}

// ── setResult ─────────────────────────────────────────────────────────────────

void HmiOperationService::setResult(uint16_t result, uint16_t errCode) {
    commandResult_  = result;
    commandErrCode_ = errCode;
    commandBusy_    = (result == kHmiResult_Busy) ? 1u : 0u;
}

// ── notifyFillComplete ───────────────────────────────────────────────────────

void HmiOperationService::notifyFillComplete(bool success) {
    lastFillResult_  = success ? kHmiResult_Done : kHmiResult_Failed;
    lastFillErrCode_ = success ? kHmiErr_None     : kHmiErr_FaultActive;
    preparedFlag_    = 0;
}

// ── executeCommand ────────────────────────────────────────────────────────────

void HmiOperationService::executeCommand(uint16_t code, uint16_t seq,
                                          StatusStore& statusStore,
                                          FillController& fillController,
                                          WeightService& weightService,
                                          SettingsStore& settingsStore) {
    const StatusSnapshot snap = statusStore.snapshot();

    // Check watchdog: if enabled and expired, refuse non-trivial commands
    if (wdtTimeout_ > 0 && hbAgeSec_ > wdtTimeout_) {
        if (code >= kHmiCmd_PrepareNext && code <= kHmiCmd_Start) {
            lastAcceptedSeq_ = seq;
            setResult(kHmiResult_Rejected, kHmiErr_Timeout);
            return;
        }
    }

    switch (code) {
        // ── Prepare next fill ──────────────────────────────────────────────
        case kHmiCmd_PrepareNext: {
            if (!snap.emergencyStopOk) {
                setResult(kHmiResult_Rejected, kHmiErr_SafetyNotReady); break;
            }
            if (snap.state == ProcessState::Fault) {
                setResult(kHmiResult_Rejected, kHmiErr_FaultActive); break;
            }
            if (!snap.weightInitialized || snap.weightReadError) {
                setResult(kHmiResult_Rejected, kHmiErr_ScaleNotReady); break;
            }
            if (!snap.calibrationValid) {
                setResult(kHmiResult_Rejected, kHmiErr_CalInvalid); break;
            }
            uint16_t errCode = kHmiErr_None;
            if (!validatePreset(errCode)) {
                presetValid_  = 0;
                presetErrCode_= errCode;
                setResult(kHmiResult_Rejected, errCode); break;
            }
            presetValid_   = 1;
            presetErrCode_ = kHmiErr_None;
            preparedFlag_  = 1;
            setResult(kHmiResult_Done, kHmiErr_None);
            break;
        }

        // ── Apply preset tare ──────────────────────────────────────────────
        case kHmiCmd_ApplyTare:
            statusStore.setTareWeight(presetTare_);
            setResult(kHmiResult_Done, kHmiErr_None);
            break;

        // ── Zero net (tare = live weight) ──────────────────────────────────
        case kHmiCmd_ZeroNet:
            statusStore.setTareWeight(snap.liveWeightKg);
            setResult(kHmiResult_Done, kHmiErr_None);
            break;

        // ── Request non-blocking HX711 tare ───────────────────────────────
        case kHmiCmd_RequestTare:
            if (snap.state != ProcessState::Idle && snap.state != ProcessState::Ready) {
                setResult(kHmiResult_Rejected, kHmiErr_Busy); break;
            }
            weightService.requestTare();
            setResult(kHmiResult_Accepted, kHmiErr_None);
            break;

        // ── Fill mode ──────────────────────────────────────────────────────
        case kHmiCmd_ModeKg:     fillMode_ = 0; setResult(kHmiResult_Done, kHmiErr_None); break;
        case kHmiCmd_ModeAmount: fillMode_ = 1; setResult(kHmiResult_Done, kHmiErr_None); break;

        // ── Start prepared fill ────────────────────────────────────────────
        case kHmiCmd_Start: {
            if (!preparedFlag_) {
                setResult(kHmiResult_Rejected, kHmiErr_NotPrepared); break;
            }
            if (!snap.emergencyStopOk) {
                setResult(kHmiResult_Rejected, kHmiErr_SafetyNotReady); break;
            }
            if (!snap.weightStable) {
                setResult(kHmiResult_Rejected, kHmiErr_Unstable); break;
            }
            // Apply preset values first
            statusStore.setTareWeight(presetTare_);
            statusStore.setTargets(presetTarget_, presetAmount_, presetRate_);
            if (presetRate_ > 0.0f) settingsStore.setRatePerKg(presetRate_);
            String reason;
            if (!fillController.startFill(presetTarget_, presetRate_, presetAmount_, reason, "hmi")) {
                setResult(kHmiResult_Failed, kHmiErr_SafetyNotReady); break;
            }
            preparedFlag_ = 0;
            setResult(kHmiResult_Accepted, kHmiErr_None);
            break;
        }

        // ── Stop fill ──────────────────────────────────────────────────────
        case kHmiCmd_Stop:
            fillController.stopFill("hmi_stop");
            setResult(kHmiResult_Done, kHmiErr_None);
            break;

        // ── Reset to idle ──────────────────────────────────────────────────
        case kHmiCmd_Reset: {
            String reason;
            fillController.resetToIdle(reason);
            preparedFlag_ = 0;
            setResult(kHmiResult_Done, kHmiErr_None);
            break;
        }

        // ── Acknowledge complete / return to idle-ready ────────────────────
        case kHmiCmd_AckComplete:
            preparedFlag_ = 0;
            setResult(kHmiResult_Done, kHmiErr_None);
            break;

        // ── Clear result ───────────────────────────────────────────────────
        case kHmiCmd_ClearResult:
            commandResult_  = kHmiResult_Idle;
            commandErrCode_ = kHmiErr_None;
            commandBusy_    = 0;
            break;

        default:
            setResult(kHmiResult_Rejected, kHmiErr_InvalidCmd);
            break;
    }
    lastAcceptedSeq_ = seq;
}

// ── pushToCache ───────────────────────────────────────────────────────────────

void HmiOperationService::pushToCache(ModbusRegisterCache& cache, const StatusSnapshot& snap) const {
    // Derived readiness flags
    const bool isFillActive = snap.state == ProcessState::FillingFast  ||
                              snap.state == ProcessState::FillingSlow  ||
                              snap.state == ProcessState::Settling     ||
                              snap.state == ProcessState::Validating;
    const bool canTare  = !isFillActive && snap.state != ProcessState::Fault;
    const bool canStop  = isFillActive;
    const bool rtp = snap.emergencyStopOk && snap.weightInitialized &&
                     !snap.weightReadError && snap.calibrationValid &&
                     (snap.state == ProcessState::Idle || snap.state == ProcessState::Ready);
    const bool rts = rtp && preparedFlag_ && snap.weightStable;

    uint16_t* h = cache.hmiRegs();
    h[0x00] = commandCode_;
    h[0x01] = commandSeq_;
    h[0x02] = lastAcceptedSeq_;
    h[0x03] = commandResult_;
    h[0x04] = commandErrCode_;
    h[0x05] = commandBusy_;
    h[0x06] = fillMode_;
    h[0x07] = preparedFlag_;
    h[0x08] = rtp ? 1u : 0u;
    h[0x09] = rts ? 1u : 0u;
    h[0x0A] = canTare ? 1u : 0u;
    h[0x0B] = canStop ? 1u : 0u;
    h[0x0C] = hbCounter_;
    h[0x0D] = wdtTimeout_;
    h[0x0E] = hbAgeSec_;
    h[0x0F] = 0;
    uint16_t hi, lo;
    floatToRegs(presetTare_,   hi, lo); h[0x10] = hi; h[0x11] = lo;
    floatToRegs(presetTarget_, hi, lo); h[0x12] = hi; h[0x13] = lo;
    floatToRegs(presetRate_,   hi, lo); h[0x14] = hi; h[0x15] = lo;
    floatToRegs(presetAmount_, hi, lo); h[0x16] = hi; h[0x17] = lo;
    h[0x18] = presetValid_;
    h[0x19] = presetErrCode_;
    h[0x1A] = lastFillResult_;
    h[0x1B] = lastFillErrCode_;
}

// ── tick (main loop) ──────────────────────────────────────────────────────────

void HmiOperationService::tick(StatusStore& statusStore, FillController& fillController,
                                WeightService& weightService, SettingsStore& settingsStore,
                                ModbusRegisterCache& cache) {
    // Update heartbeat age
    const unsigned long nowMs = millis();
    hbAgeSec_ = static_cast<uint16_t>((nowMs - lastHbMs_) / 1000UL);

    // Execute pending command
    if (pendingCmd_) {
        pendingCmd_ = false;
        executeCommand(pendingCode_, pendingSeq_, statusStore,
                       fillController, weightService, settingsStore);
    }

    // Update cache
    pushToCache(cache, statusStore.snapshot());
}
