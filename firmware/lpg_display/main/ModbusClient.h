#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "DisplaySettings.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

enum class FillState : uint8_t {
    Idle = 0, Ready, Validating, Fast, Slow, Settling,
    Complete, Aborted, Fault, Maintenance,
};

enum class CommHealth : uint8_t {
    Online = 0,
    Unstable = 1,
    Offline = 2,
};

struct ControllerSnapshot {
    float     liveWeightKg    = 0.0f;
    float     tareWeightKg    = 0.0f;
    float     netWeightKg     = 0.0f;
    float     targetWeightKg  = 0.0f;
    float     ratePerKg       = 0.0f;
    float     targetAmount    = 0.0f;
    float     currentAmount   = 0.0f;

    FillState state           = FillState::Idle;
    bool      eStopOk         = false;
    bool      cylinderPresent = false;
    bool      nozzleEngaged   = false;
    bool      weightStable    = false;

    uint16_t  todayFills      = 0;
    uint16_t  todayFails      = 0;
    float     todayKg         = 0.0f;
    float     todayAmount     = 0.0f;

    uint16_t  rtcHour         = 0;
    uint16_t  rtcMinute       = 0;
    uint16_t  rtcSecond       = 0;
    uint16_t  rtcYear         = 0;
    uint16_t  rtcMonth        = 0;
    uint16_t  rtcDay          = 0;

    uint16_t  alarmCode       = 0;
    uint16_t  alarmSeverity   = 0;
    uint16_t  readinessMask   = 0;
    uint16_t  blockerMask     = 0;

    bool      valid           = false;
    bool      connected       = false;
    CommHealth commHealth     = CommHealth::Offline;
    uint16_t  commFailStreak  = 0;
    uint16_t  commOkStreak    = 0;
    int64_t   lastOkUs        = 0;
    int64_t   lastFailUs      = 0;
};

class ModbusClient {
public:
    void begin();
    void begin(const DisplayRtuSettings& rtu);
    void applySettings(const DisplayRtuSettings& rtu);
    void poll();

    const ControllerSnapshot& snapshot() const { return snap_; }
    DisplayRtuSettings rtuSettings() const { return rtu_; }

    bool writeRegister(uint16_t reg, uint16_t val);
    bool writeRegisters(uint16_t startReg, const uint16_t* values, uint16_t count);
    bool writeFloat(uint16_t startReg, float value);
    bool readDeviceId(uint16_t& deviceId);
    bool startFill(float targetWeightKg, float ratePerKg);
    bool cmdStart()   { return writeRegisterRetry(0x0017, 1); }
    bool cmdStop()    { return writeRegisterRetry(0x0017, 2); }
    bool cmdReset()   { return writeRegisterRetry(0x0017, 3); }
    bool cmdZeroNet() { return writeRegisterRetry(0x0017, 4); }

private:
    ControllerSnapshot snap_;
    SemaphoreHandle_t busMutex_ = nullptr;
    DisplayRtuSettings rtu_;
    int64_t lastFastUs_ = 0;
    int64_t lastSlowUs_ = 0;
    int64_t lastStatUs_ = 0;
    int64_t lastDiagUs_ = 0;
    bool    bootDeviceIdChecked_ = false;
    CommHealth lastLoggedHealth_ = CommHealth::Offline;
    bool    healthLogged_ = false;

    static constexpr int64_t kFastUs = 500000;
    static constexpr int64_t kSlowUs = 3000000;
    static constexpr int64_t kStatUs = 10000000;
    static constexpr int64_t kDiagUs = 3000000;
    static constexpr int     kDefaultTimeoutMs = 500;
    static constexpr uint8_t kDefaultReadAttempts = 2;
    static constexpr uint8_t kDefaultWriteAttempts = 3;

    bool     readHR(uint16_t start, uint16_t count, uint16_t* out);
    bool     readHRRetry(uint16_t start, uint16_t count, uint16_t* out,
                         uint8_t attempts = 0);
    bool     writeRegisterRetry(uint16_t reg, uint16_t val,
                                uint8_t attempts = 0);
    bool     writeRegistersRetry(uint16_t startReg, const uint16_t* values,
                                 uint16_t count, uint8_t attempts = 0);
    bool     readPresetRegisters(float& targetWeightKg, float& ratePerKg, float& targetAmount);
    bool     confirmFillStarted(uint32_t waitMs = 900);
    void     noteCommOk();
    void     noteCommFail();
    void     updateCommHealth();
    void     logHealthIfChanged();
    bool     sendRecv(const uint8_t* req, int reqLen, uint8_t* resp, int expectLen);
    uint16_t crc16(const uint8_t* data, int len);
    static float regsToFloat(uint16_t hi, uint16_t lo);
};
