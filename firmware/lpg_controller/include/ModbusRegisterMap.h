#pragma once

#include <Arduino.h>

#include "FillController.h"
#include "RtcService.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "TransactionLog.h"

// ════════════════════════════════════════════════════════════════════════════
//  LPG Controller — Modbus Register Map
//
//  Addressing (PDU 0-based):
//    Holding Registers  FC03/FC04/FC06/FC16   40001+  → PDU 0x0000+
//    Coils              FC01/FC05              00001+  → PDU 0x0000+
//    Discrete Inputs    FC02                   10001+  → PDU 0x0000+
//
//  FLOAT32  = 2 consecutive 16-bit regs, big-endian IEEE-754 (Hi word first)
//  UINT32   = 2 consecutive 16-bit regs, big-endian (Hi word first)
//  UINT16   = 1 register
// ════════════════════════════════════════════════════════════════════════════

namespace ModbusRegisterMap {

// ── Holding Register PDU addresses (0x0000-based) ────────────────────────
//   Shown as 40001+addr in Modbus Poll / ModScan
//
constexpr uint16_t kHR_LiveWeightHi      = 0x0000;  // FLOAT32 Hi  — kg, R
constexpr uint16_t kHR_LiveWeightLo      = 0x0001;  // FLOAT32 Lo
constexpr uint16_t kHR_TareWeightHi      = 0x0002;  // FLOAT32 Hi  — kg, R/W
constexpr uint16_t kHR_TareWeightLo      = 0x0003;  // FLOAT32 Lo
constexpr uint16_t kHR_NetWeightHi       = 0x0004;  // FLOAT32 Hi  — kg, R
constexpr uint16_t kHR_NetWeightLo       = 0x0005;  // FLOAT32 Lo
constexpr uint16_t kHR_TargetWeightHi    = 0x0006;  // FLOAT32 Hi  — kg, R/W
constexpr uint16_t kHR_TargetWeightLo    = 0x0007;  // FLOAT32 Lo
constexpr uint16_t kHR_RatePerKgHi       = 0x0008;  // FLOAT32 Hi  — PKR/kg, R/W
constexpr uint16_t kHR_RatePerKgLo       = 0x0009;  // FLOAT32 Lo
constexpr uint16_t kHR_TargetAmountHi    = 0x000A;  // FLOAT32 Hi  — PKR, R/W
constexpr uint16_t kHR_TargetAmountLo    = 0x000B;  // FLOAT32 Lo
constexpr uint16_t kHR_CurrentAmountHi   = 0x000C;  // FLOAT32 Hi  — PKR, R  (net×rate)
constexpr uint16_t kHR_CurrentAmountLo   = 0x000D;  // FLOAT32 Lo
constexpr uint16_t kHR_FillState         = 0x000E;  // UINT16      — R  see enum below
constexpr uint16_t kHR_EstopOk           = 0x000F;  // UINT16      — R  1=OK
constexpr uint16_t kHR_CylinderPresent   = 0x0010;  // UINT16      — R  1=present
constexpr uint16_t kHR_NozzleEngaged     = 0x0011;  // UINT16      — R  1=engaged
constexpr uint16_t kHR_WeightStable      = 0x0012;  // UINT16      — R  1=stable
constexpr uint16_t kHR_TxnCountHi        = 0x0013;  // UINT32 Hi   — R  total transactions
constexpr uint16_t kHR_TxnCountLo        = 0x0014;  // UINT32 Lo
constexpr uint16_t kHR_UptimeHi          = 0x0015;  // UINT32 Hi   — R  seconds since boot
constexpr uint16_t kHR_UptimeLo          = 0x0016;  // UINT32 Lo
constexpr uint16_t kHR_Command           = 0x0017;  // UINT16      — W  1=Start 2=Stop 3=Reset 4=ZeroNet
constexpr uint16_t kHR_DeviceId          = 0x0018;  // UINT16      — R  0xA601

// ── Section 2: Communications (0x0019–0x001F, 7 registers) ──────────────────
constexpr uint16_t kHR_RtuSlaveAddr      = 0x0019;  // UINT16      — R/W  RTU slave addr 1–247
constexpr uint16_t kHR_RtuBaudHi         = 0x001A;  // UINT32 Hi   — R/W  RTU baud rate
constexpr uint16_t kHR_RtuBaudLo         = 0x001B;  // UINT32 Lo
constexpr uint16_t kHR_RtuParity         = 0x001C;  // UINT16      — R/W  0=N 1=E 2=O
constexpr uint16_t kHR_RtuStopBits       = 0x001D;  // UINT16      — R/W  1 or 2
constexpr uint16_t kHR_TcpPort           = 0x001E;  // UINT16      — R    always 502
constexpr uint16_t kHR_MqttConnected     = 0x001F;  // UINT16      — R    0/1

// ── Section 3: RTC / Clock (0x0020–0x0027, 8 registers) ────────────────────
constexpr uint16_t kHR_RtcYear           = 0x0020;  // UINT16      — R/W  e.g. 2025
constexpr uint16_t kHR_RtcMonth          = 0x0021;  // UINT16      — R/W  1–12
constexpr uint16_t kHR_RtcDay            = 0x0022;  // UINT16      — R/W  1–31
constexpr uint16_t kHR_RtcHour           = 0x0023;  // UINT16      — R/W  0–23
constexpr uint16_t kHR_RtcMinute         = 0x0024;  // UINT16      — R/W  0–59
constexpr uint16_t kHR_RtcSecond         = 0x0025;  // UINT16      — R/W  0–59 (write triggers RTC set)
constexpr uint16_t kHR_RtcUnixHi         = 0x0026;  // UINT32 Hi   — R/W  seconds since epoch
constexpr uint16_t kHR_RtcUnixLo         = 0x0027;  // UINT32 Lo         (write Lo triggers RTC set)

// ── Section 4: All-time statistics (0x0028–0x002F, 8 registers) ─────────────
constexpr uint16_t kHR_StatAllCompHi     = 0x0028;  // UINT32 Hi   — R  total completed fills
constexpr uint16_t kHR_StatAllCompLo     = 0x0029;  // UINT32 Lo
constexpr uint16_t kHR_StatAllFailHi     = 0x002A;  // UINT32 Hi   — R  total failed (abort+fault)
constexpr uint16_t kHR_StatAllFailLo     = 0x002B;  // UINT32 Lo
constexpr uint16_t kHR_StatAllKgHi       = 0x002C;  // FLOAT32 Hi  — R  total kg sold
constexpr uint16_t kHR_StatAllKgLo       = 0x002D;  // FLOAT32 Lo
constexpr uint16_t kHR_StatAllAmtHi      = 0x002E;  // FLOAT32 Hi  — R  total amount PKR
constexpr uint16_t kHR_StatAllAmtLo      = 0x002F;  // FLOAT32 Lo

// ── Section 5a: Today statistics (0x0030–0x0035, 6 registers) ───────────────
constexpr uint16_t kHR_StatTodayComp     = 0x0030;  // UINT16  R  completed today
constexpr uint16_t kHR_StatTodayFail     = 0x0031;  // UINT16  R  failed today
constexpr uint16_t kHR_StatTodayKgHi     = 0x0032;  // FLOAT32 R  kg today
constexpr uint16_t kHR_StatTodayKgLo     = 0x0033;
constexpr uint16_t kHR_StatTodayAmtHi    = 0x0034;  // FLOAT32 R  amount today PKR
constexpr uint16_t kHR_StatTodayAmtLo    = 0x0035;

// ── Section 5b: This week (0x0036–0x003B) ────────────────────────────────────
constexpr uint16_t kHR_StatWeekComp      = 0x0036;
constexpr uint16_t kHR_StatWeekFail      = 0x0037;
constexpr uint16_t kHR_StatWeekKgHi      = 0x0038;
constexpr uint16_t kHR_StatWeekKgLo      = 0x0039;
constexpr uint16_t kHR_StatWeekAmtHi     = 0x003A;
constexpr uint16_t kHR_StatWeekAmtLo     = 0x003B;

// ── Section 5c: This month (0x003C–0x0041) ───────────────────────────────────
constexpr uint16_t kHR_StatMonthComp     = 0x003C;
constexpr uint16_t kHR_StatMonthFail     = 0x003D;
constexpr uint16_t kHR_StatMonthKgHi     = 0x003E;
constexpr uint16_t kHR_StatMonthKgLo     = 0x003F;
constexpr uint16_t kHR_StatMonthAmtHi    = 0x0040;
constexpr uint16_t kHR_StatMonthAmtLo    = 0x0041;

// ── Section 5d: This year (0x0042–0x0047) ────────────────────────────────────
constexpr uint16_t kHR_StatYearComp      = 0x0042;
constexpr uint16_t kHR_StatYearFail      = 0x0043;
constexpr uint16_t kHR_StatYearKgHi      = 0x0044;
constexpr uint16_t kHR_StatYearKgLo      = 0x0045;
constexpr uint16_t kHR_StatYearAmtHi     = 0x0046;
constexpr uint16_t kHR_StatYearAmtLo     = 0x0047;

// Diagnostics / alarm summary
constexpr uint16_t kHR_AlarmCode         = 0x0048;  // UINT16 R  see kAlarm_* values
constexpr uint16_t kHR_AlarmSeverity     = 0x0049;  // UINT16 R  0=none 1=info 2=warning 3=alarm/fault
constexpr uint16_t kHR_ReadinessMask     = 0x004A;  // UINT16 R  bit0 estop bit1 cyl bit2 nozzle bit3 stable bit4 cal bit5 scale ready
constexpr uint16_t kHR_BlockerMask       = 0x004B;  // UINT16 R  bit0 fault bit1 estop bit2 cyl bit3 nozzle bit4 scale init bit5 read bit6 stable bit7 cal bit8 sim
constexpr uint16_t kHR_ScaleInitialized  = 0x004C;  // UINT16 R  1=HX711 initialized
constexpr uint16_t kHR_ScaleReadError    = 0x004D;  // UINT16 R  1=read error
constexpr uint16_t kHR_CalibrationValid  = 0x004E;  // UINT16 R  1=calibration valid
constexpr uint16_t kHR_SimulationActive  = 0x004F;  // UINT16 R  1=simulated weight active
constexpr uint16_t kHR_AlarmSource       = 0x0050;  // UINT16 R  0=none 1=safety 2=scale 3=process 4=operator/comms

constexpr uint16_t kHR_Base  = kHR_LiveWeightHi;
constexpr uint16_t kHR_Count = 0x0051;  // 81 registers (0x0000-0x0050)

// ── Coil PDU addresses (FC01/FC05) ───────────────────────────────────────
constexpr uint16_t kCoil_EstopOk        = 0x0000;  // R   emergency stop OK
constexpr uint16_t kCoil_CylinderPres   = 0x0001;  // R   cylinder on scale
constexpr uint16_t kCoil_NozzleEngaged  = 0x0002;  // R   gas nozzle open
constexpr uint16_t kCoil_WeightStable   = 0x0003;  // R   weight reading stable
constexpr uint16_t kCoil_FillActive     = 0x0004;  // R   any fill state active
constexpr uint16_t kCoil_Relay1         = 0x0005;  // R/W relay 1
constexpr uint16_t kCoil_Relay2         = 0x0006;  // R/W relay 2
constexpr uint16_t kCoil_Relay3         = 0x0007;  // R/W relay 3
constexpr uint16_t kCoil_Relay4         = 0x0008;  // R/W relay 4
constexpr uint16_t kCoil_Relay5         = 0x0009;  // R/W relay 5
constexpr uint16_t kCoil_Relay6         = 0x000A;  // R/W relay 6

constexpr uint16_t kCoil_Base  = kCoil_EstopOk;
constexpr uint16_t kCoil_Count = 11;

// ── Discrete Input PDU addresses (FC02) ──────────────────────────────────
constexpr uint16_t kDI_Input1 = 0x0000;
constexpr uint16_t kDI_Input2 = 0x0001;
constexpr uint16_t kDI_Input3 = 0x0002;
constexpr uint16_t kDI_Input4 = 0x0003;
constexpr uint16_t kDI_Input5 = 0x0004;
constexpr uint16_t kDI_Input6 = 0x0005;

constexpr uint16_t kDI_Base  = kDI_Input1;
constexpr uint16_t kDI_Count = 6;

// kHR_FillState values
constexpr uint16_t kFillState_Idle      = 0;
constexpr uint16_t kFillState_Ready     = 1;
constexpr uint16_t kFillState_Validating = 2;
constexpr uint16_t kFillState_Fast      = 3;
constexpr uint16_t kFillState_Slow      = 4;
constexpr uint16_t kFillState_Settling  = 5;
constexpr uint16_t kFillState_Complete  = 6;
constexpr uint16_t kFillState_Aborted   = 7;
constexpr uint16_t kFillState_Fault     = 8;
constexpr uint16_t kFillState_Maintenance = 9;

// kHR_AlarmCode values
constexpr uint16_t kAlarm_None               = 0;
constexpr uint16_t kAlarm_EmergencyStop      = 1;
constexpr uint16_t kAlarm_NozzleDisengaged   = 2;
constexpr uint16_t kAlarm_CylinderMissing    = 3;
constexpr uint16_t kAlarm_ScaleReadError     = 4;
constexpr uint16_t kAlarm_ScaleNotStable     = 5;
constexpr uint16_t kAlarm_ScaleNotCalibrated = 6;
constexpr uint16_t kAlarm_Overfill           = 7;
constexpr uint16_t kAlarm_FillTimeout        = 8;
constexpr uint16_t kAlarm_NoFlow             = 9;
constexpr uint16_t kAlarm_TransactionLog     = 10;
constexpr uint16_t kAlarm_OperatorStop       = 11;
constexpr uint16_t kAlarm_ActiveFault        = 12;
constexpr uint16_t kAlarm_ControllerOffline  = 13; // display-local only

// ── Register access functions ─────────────────────────────────────────────

// Read one holding register (PDU 0x0000-based). Returns 0 for unknown.
uint16_t readHR(uint16_t addr, const StatusSnapshot& status, const TransactionLog& txnLog,
                const SettingsStore& settings, const RtcTime& rtcTime, bool mqttConnected);

// Write one holding register. Returns false if read-only or invalid value.
bool writeHR(uint16_t addr, uint16_t value,
             StatusStore& statusStore, SettingsStore& settingsStore, FillController& fillController,
             RtcService& rtcService);

// Read one coil (returns 0 or 1). Returns 0 for unknown.
uint8_t readCoil(uint16_t addr, const StatusSnapshot& status);

// Write one coil (value = 0x0000 or 0xFF00 per Modbus spec). Returns false if read-only.
bool writeCoil(uint16_t addr, bool value, StatusStore& statusStore);

// Read one discrete input (returns 0 or 1). Returns 0 for unknown.
uint8_t readDI(uint16_t addr, const StatusSnapshot& status);

}  // namespace ModbusRegisterMap
