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
    lastActivityMs_ = millis();
    prevFillActive_ = false;
    fillStartMs_    = 0;
    fillCounter_    = 0;
    hmiCommLost_    = false;
    Serial.println(F("[HMI] HmiOperationService ready"));
}

// ── notifyModbusActivity (called by RTU/TCP service on any valid frame) ───────

void HmiOperationService::notifyModbusActivity() {
    lastActivityMs_ = millis();
    hbAgeSec_       = 0;
}

// ── pendWrite (fast path — called from Modbus write handler) ─────────────────
//
// 4x128 (offset 0x00): Command Code   — stage only, does not trigger execution
// 4x129 (offset 0x01): Command Counter — incremented per button press by HMI;
//                      firmware fires when value changes from lastAcceptedSeq_.
//                      Same-code debounce (kCommandDebouncMs) protects against
//                      comm retries for non-safety commands.
//                      Always refreshes liveness timestamp.

bool HmiOperationService::pendWrite(uint16_t hmiAddr, uint16_t value) {
    switch (hmiAddr) {
        // ── Command interface ─────────────────────────────────────────────────
        case 0x00:
            commandCode_ = value;
            return true;

        case 0x01: {
            commandSeq_ = value;
            if (value != lastAcceptedSeq_) {
                // Safety commands always fire immediately — no debounce.
                const bool safetyCmd = (commandCode_ == kHmiCmd_Stop      ||
                                        commandCode_ == kHmiCmd_Reset     ||
                                        commandCode_ == kHmiCmd_AckComplete||
                                        commandCode_ == kHmiCmd_ClearResult);
                const unsigned long nowMs = millis();
                const bool debounced = !safetyCmd &&
                                       (commandCode_ == lastExecutedCode_) &&
                                       (nowMs - lastCommandMs_ < kCommandDebouncMs);
                if (!debounced) {
                    pendingCmd_  = true;
                    pendingCode_ = commandCode_;
                    pendingSeq_  = value;
                }
            }
            // Any new counter write proves HMI is alive, even if debounced.
            notifyModbusActivity();
            return true;
        }

        // ── Fill mode ─────────────────────────────────────────────────────────
        case 0x06: fillMode_ = (value > 1) ? 0 : value; return true;

        // ── Heartbeat (backward compat — 4x140) ───────────────────────────────
        // HMI no longer needs a dedicated heartbeat macro: any Modbus read or
        // write (including status polling) already refreshes the watchdog timer.
        // This register is kept so existing programs continue to work.
        case 0x0C:
            hbCounter_++;
            notifyModbusActivity();
            return true;

        // ── Watchdog settings ────────────────────────────────────────────────
        case 0x0D: wdtTimeout_ = value; return true;
        case 0x0F: wdtMode_    = (value > 1) ? 0 : value; return true;

        // ── Preset float Hi words (staged, committed on Lo write) ─────────────
        case 0x10: stageTareHi_   = value; return true;
        case 0x11: presetTare_    = regsToFloat(stageTareHi_,   value); presetValid_ = 0; return true;
        case 0x12: stageTargetHi_ = value; return true;
        case 0x13: presetTarget_  = regsToFloat(stageTargetHi_, value); presetValid_ = 0; return true;
        case 0x14: stageRateHi_   = value; return true;
        case 0x15: presetRate_    = regsToFloat(stageRateHi_,   value); presetValid_ = 0; return true;
        case 0x16: stageAmountHi_ = value; return true;
        case 0x17: presetAmount_  = regsToFloat(stageAmountHi_, value); presetValid_ = 0; return true;

        // ── Fill record ack (4x00B0, offset 0x30) ────────────────────────────
        case 0x30: fillRecord_.pendingAck = false; return true;

        default: return false;  // read-only or unknown within HMI block
    }
}

// ── validatePreset ────────────────────────────────────────────────────────────

bool HmiOperationService::validatePreset(uint16_t& errCode) const {
    if (fillMode_ == 0) {
        if (presetTarget_ <= 0.0f || presetTarget_ > 500.0f) {
            errCode = kHmiErr_InvalidPreset; return false;
        }
    } else {
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

// ── notifyFillComplete ────────────────────────────────────────────────────────
// Retained for API compatibility with lpg_controller.ino.
// Fill record capture is now handled in tick() via state transition detection.

void HmiOperationService::notifyFillComplete(bool /*success*/) {}

// ── captureFillRecord ─────────────────────────────────────────────────────────

void HmiOperationService::captureFillRecord(const StatusSnapshot& snap, unsigned long nowMs) {
    fillRecord_.id = fillCounter_;

    // Determine outcome from final process state
    if (snap.state == ProcessState::Complete) {
        fillRecord_.result  = kHmiResult_Done;
        fillRecord_.errCode = kHmiErr_None;
    } else if (snap.state == ProcessState::Aborted ||
               snap.state == ProcessState::Idle    ||
               snap.state == ProcessState::Ready) {
        // Operator-initiated stop or reset — intentional, not a fault
        fillRecord_.result  = kHmiResult_Done;
        fillRecord_.errCode = kHmiErr_None;
    } else {
        // Fault or unexpected terminal state
        fillRecord_.result  = kHmiResult_Failed;
        fillRecord_.errCode = kHmiErr_FaultActive;
    }

    fillRecord_.mode        = fillMode_;
    fillRecord_.targetKg    = snap.targetWeightKg;
    fillRecord_.actualNetKg = snap.netWeightKg;
    fillRecord_.ratePerKg   = snap.ratePerKg;
    fillRecord_.targetAmt   = snap.targetAmount;
    fillRecord_.finalAmt    = snap.netWeightKg * snap.ratePerKg;
    fillRecord_.tareKg      = snap.tareWeightKg;
    fillRecord_.durationSec = (fillStartMs_ > 0)
                                  ? static_cast<uint32_t>((nowMs - fillStartMs_) / 1000UL)
                                  : 0;
    fillRecord_.pendingAck  = true;

    // Keep backward-compat registers (0x009A / 0x009B) in sync
    lastFillResult_  = fillRecord_.result;
    lastFillErrCode_ = fillRecord_.errCode;

    // Clear prepared state — this fill is done
    preparedFlag_ = 0;
}

// ── executeCommand ────────────────────────────────────────────────────────────

void HmiOperationService::executeCommand(uint16_t code, uint16_t seq,
                                          StatusStore& statusStore,
                                          FillController& fillController,
                                          WeightService& weightService,
                                          SettingsStore& settingsStore) {
    const StatusSnapshot snap = statusStore.snapshot();

    // Track debounce timing for this execution
    lastExecutedCode_ = code;
    lastCommandMs_    = millis();

    // Watchdog: block Start / Prepare when HMI comm is lost
    if (hmiCommLost_) {
        const bool isStartOrPrepare = (code == kHmiCmd_Start           ||
                                       code == kHmiCmd_PrepareNext     ||
                                       code == kHmiCmd_PrepareCylinder);
        if (isStartOrPrepare) {
            lastAcceptedSeq_ = seq;
            setResult(kHmiResult_Rejected, kHmiErr_CommLost);
            return;
        }
    }

    // Inline fill-active check (mirrors FillController::isActiveFillState)
    const bool fillActive = (snap.state == ProcessState::FillingFast ||
                             snap.state == ProcessState::FillingSlow ||
                             snap.state == ProcessState::Settling    ||
                             snap.state == ProcessState::Validating);

    switch (code) {

        // ── Legacy Prepare (validate preset without capturing tare) ───────────
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
                presetValid_   = 0;
                presetErrCode_ = errCode;
                setResult(kHmiResult_Rejected, errCode); break;
            }
            presetValid_   = 1;
            presetErrCode_ = kHmiErr_None;
            preparedFlag_  = 1;
            setResult(kHmiResult_Done, kHmiErr_None);
            break;
        }

        // ── Atomic Prepare Cylinder (cmd 14) ──────────────────────────────────
        // HMI writes: fillMode + targetKg + rate + amount, then sends cmd 14.
        // Firmware: checks safety, captures live weight as tare, validates
        // preset, sets preparedFlag. No HMI tare read-back required.
        case kHmiCmd_PrepareCylinder: {
            if (!snap.emergencyStopOk) {
                setResult(kHmiResult_Rejected, kHmiErr_SafetyNotReady); break;
            }
            if (snap.state == ProcessState::Fault) {
                setResult(kHmiResult_Rejected, kHmiErr_FaultActive); break;
            }
            if (fillActive) {
                setResult(kHmiResult_Rejected, kHmiErr_AlreadyFilling); break;
            }
            if (!snap.cylinderPresent) {
                setResult(kHmiResult_Rejected, kHmiErr_CylinderMissing); break;
            }
            if (!snap.weightInitialized || snap.weightReadError) {
                setResult(kHmiResult_Rejected, kHmiErr_ScaleNotReady); break;
            }
            if (!snap.calibrationValid) {
                setResult(kHmiResult_Rejected, kHmiErr_CalInvalid); break;
            }
            if (!snap.weightStable) {
                setResult(kHmiResult_Rejected, kHmiErr_Unstable); break;
            }
            // Capture live weight as tare. Clamp to 0 — scale can drift slightly negative when empty.
            presetTare_ = (snap.liveWeightKg < 0.0f) ? 0.0f : snap.liveWeightKg;
            statusStore.setTareWeight(snap.liveWeightKg);
            uint16_t errCode = kHmiErr_None;
            if (!validatePreset(errCode)) {
                presetValid_   = 0;
                presetErrCode_ = errCode;
                setResult(kHmiResult_Rejected, errCode); break;
            }
            presetValid_   = 1;
            presetErrCode_ = kHmiErr_None;
            preparedFlag_  = 1;
            setResult(kHmiResult_Done, kHmiErr_None);
            break;
        }

        // ── Apply preset tare (legacy) ─────────────────────────────────────
        case kHmiCmd_ApplyTare:
            statusStore.setTareWeight(presetTare_);
            setResult(kHmiResult_Done, kHmiErr_None);
            break;

        // ── Zero net: tare = live weight (legacy) ─────────────────────────
        case kHmiCmd_ZeroNet:
            statusStore.setTareWeight(snap.liveWeightKg);
            setResult(kHmiResult_Done, kHmiErr_None);
            break;

        // ── Non-blocking HX711 tare request ──────────────────────────────
        case kHmiCmd_RequestTare:
            if (snap.state != ProcessState::Idle && snap.state != ProcessState::Ready) {
                setResult(kHmiResult_Rejected, kHmiErr_Busy); break;
            }
            weightService.requestTare();
            setResult(kHmiResult_Accepted, kHmiErr_None);
            break;

        // ── Fill mode commands ─────────────────────────────────────────────
        case kHmiCmd_ModeKg:     fillMode_ = 0; setResult(kHmiResult_Done, kHmiErr_None); break;
        case kHmiCmd_ModeAmount: fillMode_ = 1; setResult(kHmiResult_Done, kHmiErr_None); break;

        // ── Start fill ────────────────────────────────────────────────────
        case kHmiCmd_Start: {
            if (!preparedFlag_) {
                setResult(kHmiResult_Rejected, kHmiErr_NotPrepared); break;
            }
            if (!snap.emergencyStopOk) {
                setResult(kHmiResult_Rejected, kHmiErr_SafetyNotReady); break;
            }
            // Apply validated preset to StatusStore before starting
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

        // ── Stop fill ─────────────────────────────────────────────────────
        case kHmiCmd_Stop:
            fillController.stopFill("hmi_stop");
            setResult(kHmiResult_Done, kHmiErr_None);
            break;

        // ── Reset to idle ─────────────────────────────────────────────────
        case kHmiCmd_Reset: {
            String reason;
            fillController.resetToIdle(reason);
            preparedFlag_ = 0;
            setResult(kHmiResult_Done, kHmiErr_None);
            break;
        }

        // ── Acknowledge complete ──────────────────────────────────────────
        case kHmiCmd_AckComplete:
            preparedFlag_ = 0;
            setResult(kHmiResult_Done, kHmiErr_None);
            break;

        // ── Clear command result ──────────────────────────────────────────
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
    const bool isFillActive = snap.state == ProcessState::FillingFast ||
                              snap.state == ProcessState::FillingSlow ||
                              snap.state == ProcessState::Settling    ||
                              snap.state == ProcessState::Validating;
    const bool canTare = !isFillActive && snap.state != ProcessState::Fault;
    const bool canStop = isFillActive;
    const bool rtp = snap.emergencyStopOk && snap.weightInitialized &&
                     !snap.weightReadError && snap.calibrationValid &&
                     (snap.state == ProcessState::Idle || snap.state == ProcessState::Ready);
    // weightStable was verified at cmd14 prepare time; don't re-gate here
    const bool rts = rtp && preparedFlag_;

    uint16_t* h = cache.hmiRegs();

    // ── Core command/status block (offsets 0x00–0x1B) ────────────────────────
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
    h[0x0F] = wdtMode_;       // was 0 (reserved); now watchdog mode R/W

    uint16_t hi, lo;
    floatToRegs(presetTare_,   hi, lo); h[0x10] = hi; h[0x11] = lo;
    floatToRegs(presetTarget_, hi, lo); h[0x12] = hi; h[0x13] = lo;
    floatToRegs(presetRate_,   hi, lo); h[0x14] = hi; h[0x15] = lo;
    floatToRegs(presetAmount_, hi, lo); h[0x16] = hi; h[0x17] = lo;

    h[0x18] = presetValid_;
    h[0x19] = presetErrCode_;
    h[0x1A] = lastFillResult_;   // kHR_HmiLastFillResult  (0x009A) — backward compat
    h[0x1B] = lastFillErrCode_;  // kHR_HmiLastFillErrCode (0x009B) — backward compat

    // ── Fill record block (offsets 0x1C–0x30) ────────────────────────────────
    const uint32_t recId = fillRecord_.id;
    h[0x1C] = static_cast<uint16_t>((recId >> 16) & 0xFFFF);
    h[0x1D] = static_cast<uint16_t>(recId & 0xFFFF);
    h[0x1E] = fillRecord_.result;
    h[0x1F] = fillRecord_.errCode;
    h[0x20] = fillRecord_.mode;
    floatToRegs(fillRecord_.targetKg,    hi, lo); h[0x21] = hi; h[0x22] = lo;
    floatToRegs(fillRecord_.actualNetKg, hi, lo); h[0x23] = hi; h[0x24] = lo;
    floatToRegs(fillRecord_.ratePerKg,   hi, lo); h[0x25] = hi; h[0x26] = lo;
    floatToRegs(fillRecord_.targetAmt,   hi, lo); h[0x27] = hi; h[0x28] = lo;
    floatToRegs(fillRecord_.finalAmt,    hi, lo); h[0x29] = hi; h[0x2A] = lo;
    floatToRegs(fillRecord_.tareKg,      hi, lo); h[0x2B] = hi; h[0x2C] = lo;
    const uint32_t dur = fillRecord_.durationSec;
    h[0x2D] = static_cast<uint16_t>((dur >> 16) & 0xFFFF);
    h[0x2E] = static_cast<uint16_t>(dur & 0xFFFF);
    h[0x2F] = fillRecord_.pendingAck ? 1u : 0u;
    h[0x30] = 0;  // kHR_HmiRecAck: reads as 0; write-only semantics
}

// ── tick (main loop) ──────────────────────────────────────────────────────────

void HmiOperationService::tick(StatusStore& statusStore, FillController& fillController,
                                WeightService& weightService, SettingsStore& settingsStore,
                                ModbusRegisterCache& cache) {
    const unsigned long nowMs = millis();

    // Update liveness age and derived comm-lost flag
    hbAgeSec_    = static_cast<uint16_t>((nowMs - lastActivityMs_) / 1000UL);
    hmiCommLost_ = (wdtTimeout_ > 0 && hbAgeSec_ > wdtTimeout_);

    // Execute one pending command (set by pendWrite via Modbus write handler)
    if (pendingCmd_) {
        pendingCmd_ = false;
        executeCommand(pendingCode_, pendingSeq_, statusStore,
                       fillController, weightService, settingsStore);
    }

    // Sample state AFTER command execution so fill-start is detected this tick
    const StatusSnapshot snap = statusStore.snapshot();
    const bool isNowFillActive = (snap.state == ProcessState::FillingFast ||
                                  snap.state == ProcessState::FillingSlow ||
                                  snap.state == ProcessState::Settling    ||
                                  snap.state == ProcessState::Validating);

    // ── Fill lifecycle transitions ────────────────────────────────────────────
    if (!prevFillActive_ && isNowFillActive) {
        // Fill just started (any trigger: HMI cmd30, serial, web portal)
        fillStartMs_ = nowMs;
        fillCounter_++;
    } else if (prevFillActive_ && !isNowFillActive) {
        // Fill just ended — capture final record for HMI logging
        captureFillRecord(snap, nowMs);
    }
    prevFillActive_ = isNowFillActive;

    // ── Watchdog stop-fill mode ───────────────────────────────────────────────
    // wdtMode 1: actively stop the fill when HMI comm is lost.
    // wdtMode 0 (default): only block new Start/Prepare; running fill continues.
    if (hmiCommLost_ && isNowFillActive && wdtMode_ == 1) {
        fillController.stopFill("hmi_comm_lost");
    }

    pushToCache(cache, snap);
}
