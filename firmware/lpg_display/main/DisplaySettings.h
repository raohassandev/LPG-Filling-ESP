#pragma once

#include <stdint.h>

struct DisplayRtuSettings {
    uint8_t  slaveAddress = 1;
    uint32_t baudRate = 9600;
    uint8_t  parity = 0;       // 0=None, 1=Even, 2=Odd
    uint8_t  stopBits = 1;
    uint16_t timeoutMs = 300;
    uint8_t  retries = 2;
    uint16_t unstableDebounceMs = 1500;
    uint16_t offlineDebounceMs = 5000;
};

struct DisplayIdentitySettings {
    char displayId[24] = "DSP-001";
    char stationId[24] = "LPG-STN-001";
};

struct DisplaySettingsSnapshot {
    uint16_t schemaVersion = 1;
    DisplayIdentitySettings identity;
    DisplayRtuSettings rtu;
};

class DisplaySettingsStore {
public:
    static DisplaySettingsSnapshot load();
    static bool save(const DisplaySettingsSnapshot& settings);

private:
    static void sanitize(DisplaySettingsSnapshot& settings);
};
