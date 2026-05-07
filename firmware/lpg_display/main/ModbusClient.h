#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

enum class FillState : uint8_t {
    Idle = 0, Ready, Validating, Fast, Slow, Settling,
    Complete, Aborted, Fault, Maintenance,
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

    bool      valid           = false;
    bool      connected       = false;
    int64_t   lastOkUs        = 0;
};

class ModbusClient {
public:
    void begin();
    void poll();

    const ControllerSnapshot& snapshot() const { return snap_; }

    bool writeRegister(uint16_t reg, uint16_t val);
    bool writeRegisters(uint16_t startReg, const uint16_t* values, uint16_t count);
    bool writeFloat(uint16_t startReg, float value);
    bool startFill(float targetWeightKg, float ratePerKg);
    bool cmdStart()   { return writeRegister(0x0017, 1); }
    bool cmdStop()    { return writeRegister(0x0017, 2); }
    bool cmdReset()   { return writeRegister(0x0017, 3); }
    bool cmdZeroNet() { return writeRegister(0x0017, 4); }

private:
    ControllerSnapshot snap_;
    SemaphoreHandle_t busMutex_ = nullptr;
    int64_t lastFastUs_ = 0;
    int64_t lastSlowUs_ = 0;
    int64_t lastStatUs_ = 0;

    static constexpr int64_t kFastUs = 200000;
    static constexpr int64_t kSlowUs = 2000000;
    static constexpr int64_t kStatUs = 5000000;
    static constexpr int     kTimeoutMs = 150;

    bool     readHR(uint16_t start, uint16_t count, uint16_t* out);
    bool     sendRecv(const uint8_t* req, int reqLen, uint8_t* resp, int expectLen);
    uint16_t crc16(const uint8_t* data, int len);
    static float regsToFloat(uint16_t hi, uint16_t lo);
};
