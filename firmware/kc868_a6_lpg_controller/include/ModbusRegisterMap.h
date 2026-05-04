#pragma once

#include <Arduino.h>

#include "FillController.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "TransactionLog.h"

namespace ModbusRegisterMap {

// ── Holding register addresses (all 0x1001–0x100F) ───────────────────────────
constexpr uint16_t kLiveWeight       = 0x1001;  // kg×100  R
constexpr uint16_t kTareWeight       = 0x1002;  // kg×100  R/W
constexpr uint16_t kNetWeight        = 0x1003;  // kg×100  R
constexpr uint16_t kFillingStatus    = 0x1004;  // enum    R   0=IDLE 1=FAST 2=SLOW 3=SETTLING 4=COMPLETE 5=FAULT
constexpr uint16_t kTargetWeight     = 0x1005;  // kg×100  R/W
constexpr uint16_t kEstopStatus      = 0x1006;  // 0/1     R   1=OK
constexpr uint16_t kRatePerKg        = 0x1007;  // PKR×100 R/W e.g. 25000 = 250.00 PKR/kg
constexpr uint16_t kTargetAmount     = 0x1008;  // PKR     R/W integer
constexpr uint16_t kCurrentAmount    = 0x1009;  // PKR     R   running total
constexpr uint16_t kTransactionCount = 0x100A;  // count   R
constexpr uint16_t kCylinderPresent  = 0x100B;  // 0/1     R
constexpr uint16_t kNozzleEngaged    = 0x100C;  // 0/1     R
constexpr uint16_t kWeightStable     = 0x100D;  // 0/1     R
constexpr uint16_t kUptimeSec        = 0x100E;  // seconds R   wraps at 65535
constexpr uint16_t kCommand          = 0x100F;  // —       W   1=Start 2=Stop 3=Reset 4=ZeroNet (always reads 0)

constexpr uint16_t kRegisterBase  = kLiveWeight;
constexpr uint16_t kRegisterCount = 15;  // 0x1001–0x100F inclusive

// Read one holding register; returns 0 for unknown addresses.
uint16_t readHoldingRegister(uint16_t address,
                              const StatusSnapshot& status,
                              const TransactionLog& txnLog);

// Write one holding register. Returns false if address is read-only or unknown.
bool writeHoldingRegister(uint16_t address, uint16_t value,
                           StatusStore& statusStore,
                           SettingsStore& settingsStore,
                           FillController& fillController);

}  // namespace ModbusRegisterMap
