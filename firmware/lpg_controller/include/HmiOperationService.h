#pragma once

#include <Arduino.h>
#include "FillController.h"
#include "ModbusRegisterCache.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "WeightService.h"

// ── HMI command codes ────────────────────────────────────────────────────────
constexpr uint16_t kHmiCmd_None         = 0;
constexpr uint16_t kHmiCmd_PrepareNext  = 10;
constexpr uint16_t kHmiCmd_ApplyTare    = 11;
constexpr uint16_t kHmiCmd_ZeroNet      = 12;
constexpr uint16_t kHmiCmd_RequestTare  = 13;
constexpr uint16_t kHmiCmd_ModeKg       = 20;
constexpr uint16_t kHmiCmd_ModeAmount   = 21;
constexpr uint16_t kHmiCmd_Start        = 30;
constexpr uint16_t kHmiCmd_Stop         = 31;
constexpr uint16_t kHmiCmd_Reset        = 32;
constexpr uint16_t kHmiCmd_AckComplete  = 33;
constexpr uint16_t kHmiCmd_ClearResult  = 40;

// ── HMI result codes ─────────────────────────────────────────────────────────
constexpr uint16_t kHmiResult_Idle      = 0;
constexpr uint16_t kHmiResult_Accepted  = 1;
constexpr uint16_t kHmiResult_Busy      = 2;
constexpr uint16_t kHmiResult_Rejected  = 3;
constexpr uint16_t kHmiResult_Done      = 4;
constexpr uint16_t kHmiResult_Failed    = 5;

// ── HMI error codes ──────────────────────────────────────────────────────────
constexpr uint16_t kHmiErr_None           = 0;
constexpr uint16_t kHmiErr_InvalidCmd     = 1;
constexpr uint16_t kHmiErr_InvalidSeq     = 2;
constexpr uint16_t kHmiErr_Busy           = 3;
constexpr uint16_t kHmiErr_NotPrepared    = 4;
constexpr uint16_t kHmiErr_SafetyNotReady = 5;
constexpr uint16_t kHmiErr_ScaleNotReady  = 6;
constexpr uint16_t kHmiErr_Unstable       = 7;
constexpr uint16_t kHmiErr_CalInvalid     = 8;
constexpr uint16_t kHmiErr_InvalidPreset  = 9;
constexpr uint16_t kHmiErr_NotAllowed     = 10;
constexpr uint16_t kHmiErr_Timeout        = 11;
constexpr uint16_t kHmiErr_FaultActive    = 12;

// ─────────────────────────────────────────────────────────────────────────────
//  HmiOperationService
//
//  Holds HMI preset + command state in RAM.
//  pendWrite() is called from the Modbus write handler (fast path — returns immediately).
//  tick() is called from the main loop and executes any pending command.
// ─────────────────────────────────────────────────────────────────────────────
class HmiOperationService {
public:
    void begin();

    // Called from Modbus write handler (fast path).
    // hmiAddr: 0-based within HMI block (kHR_HmiBase subtracted by caller).
    // Returns false if the address or value is clearly invalid.
    bool pendWrite(uint16_t hmiAddr, uint16_t value);

    // Called from main loop. Executes one pending command, then updates cache.
    void tick(StatusStore& statusStore, FillController& fillController,
              WeightService& weightService, SettingsStore& settingsStore,
              ModbusRegisterCache& cache);

    // Notify of fill completion (called by ino loop on state transition).
    void notifyFillComplete(bool success);

private:
    // ── Pending command (set by pendWrite, consumed by tick) ────────────────
    bool     pendingCmd_   = false;
    uint16_t pendingCode_  = 0;
    uint16_t pendingSeq_   = 0;

    // ── Staging registers for Hi-word of Float32 writes ─────────────────────
    uint16_t stageTareHi_   = 0;
    uint16_t stageTargetHi_ = 0;
    uint16_t stageRateHi_   = 0;
    uint16_t stageAmountHi_ = 0;

    // ── Command tracking ────────────────────────────────────────────────────
    uint16_t commandCode_       = 0;
    uint16_t commandSeq_        = 0;
    uint16_t lastAcceptedSeq_   = 0xFFFF;
    uint16_t commandResult_     = kHmiResult_Idle;
    uint16_t commandErrCode_    = kHmiErr_None;
    uint16_t commandBusy_       = 0;

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

    // ── Heartbeat / watchdog ─────────────────────────────────────────────────
    uint16_t      hbCounter_     = 0;
    uint16_t      wdtTimeout_    = 30;  // seconds, 0=disabled
    unsigned long lastHbMs_      = 0;
    uint16_t      hbAgeSec_      = 0;

    // ── Last fill result ─────────────────────────────────────────────────────
    uint16_t lastFillResult_  = kHmiResult_Idle;
    uint16_t lastFillErrCode_ = kHmiErr_None;

    // ── Helpers ──────────────────────────────────────────────────────────────
    void executeCommand(uint16_t code, uint16_t seq,
                        StatusStore& statusStore, FillController& fillController,
                        WeightService& weightService, SettingsStore& settingsStore);
    bool validatePreset(uint16_t& errCode) const;
    void setResult(uint16_t result, uint16_t errCode);
    void pushToCache(ModbusRegisterCache& cache, const StatusSnapshot& snap) const;

    static float regsToFloat(uint16_t hi, uint16_t lo);
    static void  floatToRegs(float f, uint16_t& hi, uint16_t& lo);
};
