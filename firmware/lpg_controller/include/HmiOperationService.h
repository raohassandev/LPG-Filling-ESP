#pragma once

#include <Arduino.h>
#include "FillController.h"
#include "ModbusRegisterCache.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "WeightService.h"

// ── HMI command codes ────────────────────────────────────────────────────────
constexpr uint16_t kHmiCmd_None            = 0;
constexpr uint16_t kHmiCmd_PrepareNext     = 10;  // legacy: validate preset only
constexpr uint16_t kHmiCmd_ApplyTare       = 11;
constexpr uint16_t kHmiCmd_ZeroNet         = 12;
constexpr uint16_t kHmiCmd_RequestTare     = 13;
constexpr uint16_t kHmiCmd_PrepareCylinder = 14;  // atomic: capture tare + validate + prepare
constexpr uint16_t kHmiCmd_ModeKg          = 20;
constexpr uint16_t kHmiCmd_ModeAmount      = 21;
constexpr uint16_t kHmiCmd_Start           = 30;
constexpr uint16_t kHmiCmd_Stop            = 31;
constexpr uint16_t kHmiCmd_Reset           = 32;
constexpr uint16_t kHmiCmd_AckComplete     = 33;
constexpr uint16_t kHmiCmd_ClearResult     = 40;

// ── HMI result codes ─────────────────────────────────────────────────────────
constexpr uint16_t kHmiResult_Idle     = 0;
constexpr uint16_t kHmiResult_Accepted = 1;
constexpr uint16_t kHmiResult_Busy     = 2;
constexpr uint16_t kHmiResult_Rejected = 3;
constexpr uint16_t kHmiResult_Done     = 4;
constexpr uint16_t kHmiResult_Failed   = 5;

// ── HMI error codes ──────────────────────────────────────────────────────────
constexpr uint16_t kHmiErr_None             = 0;
constexpr uint16_t kHmiErr_InvalidCmd       = 1;
constexpr uint16_t kHmiErr_InvalidSeq       = 2;
constexpr uint16_t kHmiErr_Busy             = 3;
constexpr uint16_t kHmiErr_NotPrepared      = 4;
constexpr uint16_t kHmiErr_SafetyNotReady   = 5;
constexpr uint16_t kHmiErr_ScaleNotReady    = 6;
constexpr uint16_t kHmiErr_Unstable         = 7;
constexpr uint16_t kHmiErr_CalInvalid       = 8;
constexpr uint16_t kHmiErr_InvalidPreset    = 9;
constexpr uint16_t kHmiErr_NotAllowed       = 10;
constexpr uint16_t kHmiErr_Timeout          = 11;
constexpr uint16_t kHmiErr_FaultActive      = 12;
constexpr uint16_t kHmiErr_AlreadyFilling   = 13;
constexpr uint16_t kHmiErr_CylinderMissing  = 14;
constexpr uint16_t kHmiErr_NozzleNotEngaged = 15;
constexpr uint16_t kHmiErr_CommLost         = 16;

// ── Fill record: published on every fill completion for HMI logging ──────────
struct HmiFillRecord {
    uint32_t id          = 0;     // monotonic fill counter (1, 2, 3, …)
    uint16_t result      = 0;     // kHmiResult_*
    uint16_t errCode     = 0;     // kHmiErr_*
    uint16_t mode        = 0;     // 0=by-kg 1=by-amount
    float    targetKg    = 0.0f;
    float    actualNetKg = 0.0f;
    float    ratePerKg   = 0.0f;
    float    targetAmt   = 0.0f;
    float    finalAmt    = 0.0f;  // actualNetKg × ratePerKg
    float    tareKg      = 0.0f;
    uint32_t durationSec = 0;
    bool     pendingAck  = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  HmiOperationService
//
//  Architecture:
//  • Liveness: any valid Modbus frame from the master refreshes lastActivityMs_
//    via notifyModbusActivity(). The 4x140 heartbeat register still works for
//    backward compatibility but is no longer required.
//  • Command trigger: 4x129 is a request counter the HMI increments per button
//    press. Firmware fires when the counter changes; same-code debounce (300 ms)
//    protects against comm retries for non-safety commands.
//  • Watchdog modes (4x008F): 0 = warn-only (block Start/Prepare only),
//    1 = stop-fill (also stops an active fill when comm is lost).
//  • Command 14 = atomic Prepare Cylinder: captures live weight as tare, validates
//    preset, sets preparedFlag without any HMI read-back loop.
//  • Fill record (4x009C–4x00B0): populated on every fill end; HMI acks via
//    writing 4x00B0.
// ─────────────────────────────────────────────────────────────────────────────
class HmiOperationService {
public:
    void begin();

    // Called from any Modbus service after a valid frame is received.
    // Refreshes the activity timestamp used by the watchdog.
    void notifyModbusActivity();

    // Called from Modbus write handler (fast path — returns immediately).
    // hmiAddr: 0-based within HMI block (kHR_HmiBase subtracted by caller).
    bool pendWrite(uint16_t hmiAddr, uint16_t value);

    // Called from main loop. Executes one pending command and updates cache.
    void tick(StatusStore& statusStore, FillController& fillController,
              WeightService& weightService, SettingsStore& settingsStore,
              ModbusRegisterCache& cache);

    // Retained for API compatibility with lpg_controller.ino.
    // Fill record capture is now handled internally in tick().
    void notifyFillComplete(bool success);

private:
    // ── Pending command (set by pendWrite, consumed by tick) ────────────────
    bool     pendingCmd_  = false;
    uint16_t pendingCode_ = 0;
    uint16_t pendingSeq_  = 0;

    // ── Staging registers for Hi-word of Float32 writes ─────────────────────
    uint16_t stageTareHi_   = 0;
    uint16_t stageTargetHi_ = 0;
    uint16_t stageRateHi_   = 0;
    uint16_t stageAmountHi_ = 0;

    // ── Command tracking ────────────────────────────────────────────────────
    uint16_t commandCode_     = 0;
    uint16_t commandSeq_      = 0;
    uint16_t lastAcceptedSeq_ = 0xFFFF;
    uint16_t commandResult_   = kHmiResult_Idle;
    uint16_t commandErrCode_  = kHmiErr_None;
    uint16_t commandBusy_     = 0;

    // ── Same-code command debounce ───────────────────────────────────────────
    uint16_t      lastExecutedCode_ = 0;
    unsigned long lastCommandMs_    = 0;
    static constexpr unsigned long kCommandDebouncMs = 300;

    // ── Fill mode and prepared state ─────────────────────────────────────────
    uint16_t fillMode_     = 0;  // 0=by-kg, 1=by-amount
    uint16_t preparedFlag_ = 0;
    uint16_t presetValid_  = 0;
    uint16_t presetErrCode_= 0;

    // ── Preset values ────────────────────────────────────────────────────────
    float presetTare_   = 0.0f;
    float presetTarget_ = 12.0f;
    float presetRate_   = 250.0f;
    float presetAmount_ = 3000.0f;

    // ── Liveness / watchdog ──────────────────────────────────────────────────
    uint16_t      hbCounter_      = 0;        // backward-compat heartbeat write counter
    uint16_t      wdtTimeout_     = 30;       // seconds; 0 = disabled
    uint16_t      wdtMode_        = 0;        // 0=warn-only, 1=stop-fill
    unsigned long lastActivityMs_ = 0;        // refreshed by any valid Modbus frame
    uint16_t      hbAgeSec_       = 0;        // seconds since last activity (read-only cache)
    bool          hmiCommLost_    = false;    // derived: wdtTimeout>0 && hbAgeSec>wdtTimeout

    // ── Fill lifecycle tracking ──────────────────────────────────────────────
    bool          prevFillActive_ = false;
    unsigned long fillStartMs_    = 0;
    uint32_t      fillCounter_    = 0;    // monotonic: incremented at each fill start

    // ── Fill record ──────────────────────────────────────────────────────────
    HmiFillRecord fillRecord_;

    // ── Last fill result (backward-compat registers 0x009A/0x009B) ──────────
    uint16_t lastFillResult_  = kHmiResult_Idle;
    uint16_t lastFillErrCode_ = kHmiErr_None;

    // ── Helpers ──────────────────────────────────────────────────────────────
    void executeCommand(uint16_t code, uint16_t seq,
                        StatusStore& statusStore, FillController& fillController,
                        WeightService& weightService, SettingsStore& settingsStore);
    bool validatePreset(uint16_t& errCode) const;
    void setResult(uint16_t result, uint16_t errCode);
    void captureFillRecord(const StatusSnapshot& snap, unsigned long nowMs);
    void pushToCache(ModbusRegisterCache& cache, const StatusSnapshot& snap) const;

    static float   regsToFloat(uint16_t hi, uint16_t lo);
    static void    floatToRegs(float f, uint16_t& hi, uint16_t& lo);
};
