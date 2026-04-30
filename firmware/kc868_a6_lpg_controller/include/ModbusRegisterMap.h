#pragma once

#include <Arduino.h>

#include "StatusStore.h"

namespace ModbusRegisterMap {
constexpr uint16_t kLiveWeight = 0x1001;
constexpr uint16_t kTareWeight = 0x1002;
constexpr uint16_t kNetWeight = 0x1003;
constexpr uint16_t kFillingStatus = 0x1004;
constexpr uint16_t kTargetWeight = 0x1005;
constexpr uint16_t kEstopStatus = 0x1006;
constexpr uint16_t kRegisterCount = 6;

// Weight registers are kg x 100 so an HMI can poll fast 16-bit holding registers.
uint16_t readHoldingRegister(uint16_t address, const StatusSnapshot& status);
bool writeHoldingRegister(uint16_t address, uint16_t value, StatusStore& statusStore);
}  // namespace ModbusRegisterMap
