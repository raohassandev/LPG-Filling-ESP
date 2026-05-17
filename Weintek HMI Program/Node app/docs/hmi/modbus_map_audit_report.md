# Modbus Map Audit Report

Generated: 2026-05-14T06:53:24.320Z
Firmware source: `D:\Working\LPG-Filling-ESP\firmware\lpg_controller\include\ModbusRegisterMap.h`

**No issues found.**

## Register Table

| Name | PDU Addr (hex) | 40001+ | Type | Words | Comment |
|---|---|---|---|---|---|
| kHR_LiveWeightHi | 0x0000 | 40001 | FLOAT32 | 2 | FLOAT32 Hi  — kg, R |
| kHR_LiveWeightLo | 0x0001 | 40002 | FLOAT32_LO | 1 | FLOAT32 Lo |
| kHR_TareWeightHi | 0x0002 | 40003 | FLOAT32 | 2 | FLOAT32 Hi  — kg, R/W |
| kHR_TareWeightLo | 0x0003 | 40004 | FLOAT32_LO | 1 | FLOAT32 Lo |
| kHR_NetWeightHi | 0x0004 | 40005 | FLOAT32 | 2 | FLOAT32 Hi  — kg, R |
| kHR_NetWeightLo | 0x0005 | 40006 | FLOAT32_LO | 1 | FLOAT32 Lo |
| kHR_TargetWeightHi | 0x0006 | 40007 | FLOAT32 | 2 | FLOAT32 Hi  — kg, R/W |
| kHR_TargetWeightLo | 0x0007 | 40008 | FLOAT32_LO | 1 | FLOAT32 Lo |
| kHR_RatePerKgHi | 0x0008 | 40009 | FLOAT32 | 2 | FLOAT32 Hi  — PKR/kg, R/W |
| kHR_RatePerKgLo | 0x0009 | 40010 | FLOAT32_LO | 1 | FLOAT32 Lo |
| kHR_TargetAmountHi | 0x000A | 40011 | FLOAT32 | 2 | FLOAT32 Hi  — PKR, R/W |
| kHR_TargetAmountLo | 0x000B | 40012 | FLOAT32_LO | 1 | FLOAT32 Lo |
| kHR_CurrentAmountHi | 0x000C | 40013 | FLOAT32 | 2 | FLOAT32 Hi  — PKR, R  (net×rate) |
| kHR_CurrentAmountLo | 0x000D | 40014 | FLOAT32_LO | 1 | FLOAT32 Lo |
| kHR_FillState | 0x000E | 40015 | INT16 | 1 | UINT16      — R  see enum below |
| kHR_EstopOk | 0x000F | 40016 | INT16 | 1 | UINT16      — R  1=OK |
| kHR_CylinderPresent | 0x0010 | 40017 | INT16 | 1 | UINT16      — R  1=present |
| kHR_NozzleEngaged | 0x0011 | 40018 | INT16 | 1 | UINT16      — R  1=engaged |
| kHR_WeightStable | 0x0012 | 40019 | INT16 | 1 | UINT16      — R  1=stable |
| kHR_TxnCountHi | 0x0013 | 40020 | UINT32 | 2 | UINT32 Hi   — R  total transactions |
| kHR_TxnCountLo | 0x0014 | 40021 | UINT32_LO | 1 | UINT32 Lo |
| kHR_UptimeHi | 0x0015 | 40022 | UINT32 | 2 | UINT32 Hi   — R  seconds since boot |
| kHR_UptimeLo | 0x0016 | 40023 | UINT32_LO | 1 | UINT32 Lo |
| kHR_Command | 0x0017 | 40024 | INT16 | 1 | UINT16      — W  1=Start 2=Stop 3=Reset 4=ZeroNet |
| kHR_DeviceId | 0x0018 | 40025 | INT16 | 1 | UINT16      — R  0xA601 |
| kHR_RtuSlaveAddr | 0x0019 | 40026 | INT16 | 1 | UINT16      — R/W  RTU slave addr 1–247 |
| kHR_RtuBaudHi | 0x001A | 40027 | UINT32 | 2 | UINT32 Hi   — R/W  RTU baud rate |
| kHR_RtuBaudLo | 0x001B | 40028 | UINT32_LO | 1 | UINT32 Lo |
| kHR_RtuParity | 0x001C | 40029 | INT16 | 1 | UINT16      — R/W  0=N 1=E 2=O |
| kHR_RtuStopBits | 0x001D | 40030 | INT16 | 1 | UINT16      — R/W  1 or 2 |
| kHR_TcpPort | 0x001E | 40031 | INT16 | 1 | UINT16      — R    always 502 |
| kHR_MqttConnected | 0x001F | 40032 | INT16 | 1 | UINT16      — R    0/1 |
| kHR_RtcYear | 0x0020 | 40033 | INT16 | 1 | UINT16      — R/W  e.g. 2025 |
| kHR_RtcMonth | 0x0021 | 40034 | INT16 | 1 | UINT16      — R/W  1–12 |
| kHR_RtcDay | 0x0022 | 40035 | INT16 | 1 | UINT16      — R/W  1–31 |
| kHR_RtcHour | 0x0023 | 40036 | INT16 | 1 | UINT16      — R/W  0–23 |
| kHR_RtcMinute | 0x0024 | 40037 | INT16 | 1 | UINT16      — R/W  0–59 |
| kHR_RtcSecond | 0x0025 | 40038 | INT16 | 1 | UINT16      — R/W  0–59 (write triggers RTC set) |
| kHR_RtcUnixHi | 0x0026 | 40039 | UINT32 | 2 | UINT32 Hi   — R/W  seconds since epoch |
| kHR_RtcUnixLo | 0x0027 | 40040 | UINT32_LO | 1 | UINT32 Lo         (write Lo triggers RTC set) |
| kHR_StatAllCompHi | 0x0028 | 40041 | UINT32 | 2 | UINT32 Hi   — R  total completed fills |
| kHR_StatAllCompLo | 0x0029 | 40042 | UINT32_LO | 1 | UINT32 Lo |
| kHR_StatAllFailHi | 0x002A | 40043 | UINT32 | 2 | UINT32 Hi   — R  total failed (abort+fault) |
| kHR_StatAllFailLo | 0x002B | 40044 | UINT32_LO | 1 | UINT32 Lo |
| kHR_StatAllKgHi | 0x002C | 40045 | FLOAT32 | 2 | FLOAT32 Hi  — R  total kg sold |
| kHR_StatAllKgLo | 0x002D | 40046 | FLOAT32_LO | 1 | FLOAT32 Lo |
| kHR_StatAllAmtHi | 0x002E | 40047 | FLOAT32 | 2 | FLOAT32 Hi  — R  total amount PKR |
| kHR_StatAllAmtLo | 0x002F | 40048 | FLOAT32_LO | 1 | FLOAT32 Lo |
| kHR_StatTodayComp | 0x0030 | 40049 | INT16 | 1 | UINT16  R  completed today |
| kHR_StatTodayFail | 0x0031 | 40050 | INT16 | 1 | UINT16  R  failed today |
| kHR_StatTodayKgHi | 0x0032 | 40051 | UINT16 | 1 | FLOAT32 R  kg today |
| kHR_StatTodayKgLo | 0x0033 | 40052 | UINT16 | 1 |  |
| kHR_StatTodayAmtHi | 0x0034 | 40053 | UINT16 | 1 | FLOAT32 R  amount today PKR |
| kHR_StatTodayAmtLo | 0x0035 | 40054 | UINT16 | 1 |  |
| kHR_StatWeekComp | 0x0036 | 40055 | UINT16 | 1 |  |
| kHR_StatWeekFail | 0x0037 | 40056 | UINT16 | 1 |  |
| kHR_StatWeekKgHi | 0x0038 | 40057 | UINT16 | 1 |  |
| kHR_StatWeekKgLo | 0x0039 | 40058 | UINT16 | 1 |  |
| kHR_StatWeekAmtHi | 0x003A | 40059 | UINT16 | 1 |  |
| kHR_StatWeekAmtLo | 0x003B | 40060 | UINT16 | 1 |  |
| kHR_StatMonthComp | 0x003C | 40061 | UINT16 | 1 |  |
| kHR_StatMonthFail | 0x003D | 40062 | UINT16 | 1 |  |
| kHR_StatMonthKgHi | 0x003E | 40063 | UINT16 | 1 |  |
| kHR_StatMonthKgLo | 0x003F | 40064 | UINT16 | 1 |  |
| kHR_StatMonthAmtHi | 0x0040 | 40065 | UINT16 | 1 |  |
| kHR_StatMonthAmtLo | 0x0041 | 40066 | UINT16 | 1 |  |
| kHR_StatYearComp | 0x0042 | 40067 | UINT16 | 1 |  |
| kHR_StatYearFail | 0x0043 | 40068 | UINT16 | 1 |  |
| kHR_StatYearKgHi | 0x0044 | 40069 | UINT16 | 1 |  |
| kHR_StatYearKgLo | 0x0045 | 40070 | UINT16 | 1 |  |
| kHR_StatYearAmtHi | 0x0046 | 40071 | UINT16 | 1 |  |
| kHR_StatYearAmtLo | 0x0047 | 40072 | UINT16 | 1 |  |
| kHR_AlarmCode | 0x0048 | 40073 | INT16 | 1 | UINT16 R  see kAlarm_* values |
| kHR_AlarmSeverity | 0x0049 | 40074 | INT16 | 1 | UINT16 R  0=none 1=info 2=warning 3=alarm/fault |
| kHR_ReadinessMask | 0x004A | 40075 | INT16 | 1 | UINT16 R  bit0 estop bit1 cyl bit2 nozzle bit3 stable bit4 cal bit5 scale ready |
| kHR_BlockerMask | 0x004B | 40076 | INT16 | 1 | UINT16 R  bit0 fault bit1 estop bit2 cyl bit3 nozzle bit4 scale init bit5 read bit6 stable bit7 cal bit8 sim |
| kHR_ScaleInitialized | 0x004C | 40077 | INT16 | 1 | UINT16 R  1=HX711 initialized |
| kHR_ScaleReadError | 0x004D | 40078 | INT16 | 1 | UINT16 R  1=read error |
| kHR_CalibrationValid | 0x004E | 40079 | INT16 | 1 | UINT16 R  1=calibration valid |
| kHR_SimulationActive | 0x004F | 40080 | INT16 | 1 | UINT16 R  1=simulated weight active |
| kHR_AlarmSource | 0x0050 | 40081 | INT16 | 1 | UINT16 R  0=none 1=safety 2=scale 3=process 4=operator/comms |
| kHR_ApplicationLoadHi | 0x0051 | 40082 | UINT16 | 1 | FLOAT32 R  main-loop load estimate percent |
| kHR_ApplicationLoadLo | 0x0052 | 40083 | UINT16 | 1 |  |
| kHR_LoopAvgMsHi | 0x0053 | 40084 | UINT16 | 1 | FLOAT32 R  milliseconds |
| kHR_LoopAvgMsLo | 0x0054 | 40085 | UINT16 | 1 |  |
| kHR_LoopMaxMsHi | 0x0055 | 40086 | UINT16 | 1 | FLOAT32 R  milliseconds |
| kHR_LoopMaxMsLo | 0x0056 | 40087 | UINT16 | 1 |  |
| kHR_HeapTotalHi | 0x0057 | 40088 | UINT16 | 1 | UINT32 R   bytes |
| kHR_HeapTotalLo | 0x0058 | 40089 | UINT16 | 1 |  |
| kHR_HeapFreeHi | 0x0059 | 40090 | UINT16 | 1 | UINT32 R   bytes |
| kHR_HeapFreeLo | 0x005A | 40091 | UINT16 | 1 |  |
| kHR_HeapMinFreeHi | 0x005B | 40092 | UINT16 | 1 | UINT32 R   bytes |
| kHR_HeapMinFreeLo | 0x005C | 40093 | UINT16 | 1 |  |
| kHR_HeapFreePctHi | 0x005D | 40094 | UINT16 | 1 | FLOAT32 R  percent |
| kHR_HeapFreePctLo | 0x005E | 40095 | UINT16 | 1 |  |
| kHR_PsramTotalHi | 0x005F | 40096 | UINT16 | 1 | UINT32 R   bytes |
| kHR_PsramTotalLo | 0x0060 | 40097 | UINT16 | 1 |  |
| kHR_PsramFreeHi | 0x0061 | 40098 | UINT16 | 1 | UINT32 R   bytes |
| kHR_PsramFreeLo | 0x0062 | 40099 | UINT16 | 1 |  |
| kHR_PsramFreePctHi | 0x0063 | 40100 | UINT16 | 1 | FLOAT32 R  percent |
| kHR_PsramFreePctLo | 0x0064 | 40101 | UINT16 | 1 |  |
| kHR_FlashSizeHi | 0x0065 | 40102 | UINT16 | 1 | UINT32 R   bytes |
| kHR_FlashSizeLo | 0x0066 | 40103 | UINT16 | 1 |  |
| kHR_SketchSizeHi | 0x0067 | 40104 | UINT16 | 1 | UINT32 R   bytes |
| kHR_SketchSizeLo | 0x0068 | 40105 | UINT16 | 1 |  |
| kHR_FreeSketchHi | 0x0069 | 40106 | UINT16 | 1 | UINT32 R   bytes |
| kHR_FreeSketchLo | 0x006A | 40107 | UINT16 | 1 |  |
| kHR_ChipTempHi | 0x006B | 40108 | UINT16 | 1 | FLOAT32 R  ESP32 internal chip temperature C |
| kHR_ChipTempLo | 0x006C | 40109 | UINT16 | 1 |  |
| kHR_WifiRssi | 0x006D | 40110 | INT16 | 1 | INT16 R    dBm |
| kHR_WifiStatus | 0x006E | 40111 | INT16 | 1 | UINT16 R   ESP32 WiFi status |
| kHR_MqttClientState | 0x006F | 40112 | INT16 | 1 | INT16 R    0 disconnected, 1 connected |
| kHR_LastResetReason | 0x0070 | 40113 | INT16 | 1 | UINT16 R   esp_reset_reason |
| kHR_FirmwareBuildMode | 0x0071 | 40114 | INT16 | 1 | UINT16 R   0 production, 1 prototype, 2 development |
| kHR_RtuReqCountHi | 0x0072 | 40115 | UINT16 | 1 | UINT32 R |
| kHR_RtuReqCountLo | 0x0073 | 40116 | UINT16 | 1 |  |
| kHR_RtuErrCountHi | 0x0074 | 40117 | UINT16 | 1 | UINT32 R |
| kHR_RtuErrCountLo | 0x0075 | 40118 | UINT16 | 1 |  |
| kHR_HeartbeatHi | 0x0076 | 40119 | UINT16 | 1 | UINT32 R |
| kHR_HeartbeatLo | 0x0077 | 40120 | UINT16 | 1 |  |
| kHR_HmiCommandCode | 0x0080 | 40129 | INT16 | 1 | UINT16 R/W  command code (kHmiCmd_*) |
| kHR_HmiCommandSeq | 0x0081 | 40130 | INT16 | 1 | UINT16 R/W  increment per new command |
| kHR_HmiLastAcceptedSeq | 0x0082 | 40131 | INT16 | 1 | UINT16 R    echo of last accepted seq |
| kHR_HmiCommandResult | 0x0083 | 40132 | INT16 | 1 | UINT16 R    kHmiResult_* |
| kHR_HmiCommandErrCode | 0x0084 | 40133 | INT16 | 1 | UINT16 R    kHmiErr_* |
| kHR_HmiCommandBusy | 0x0085 | 40134 | INT16 | 1 | UINT16 R    1=controller processing |
| kHR_HmiFillMode | 0x0086 | 40135 | INT16 | 1 | UINT16 R/W  0=by-kg 1=by-amount |
| kHR_HmiPreparedFlag | 0x0087 | 40136 | INT16 | 1 | UINT16 R    1=preset validated, ready |
| kHR_HmiReadyToPrepare | 0x0088 | 40137 | INT16 | 1 | UINT16 R    1=safety+scale OK |
| kHR_HmiReadyToStart | 0x0089 | 40138 | INT16 | 1 | UINT16 R    1=prepared+safe+stable |
| kHR_HmiCanTare | 0x008A | 40139 | INT16 | 1 | UINT16 R    1=tare safe now |
| kHR_HmiCanStop | 0x008B | 40140 | INT16 | 1 | UINT16 R    1=fill active |
| kHR_HmiHeartbeat | 0x008C | 40141 | INT16 | 1 | UINT16 R/W  HMI writes to prove alive |
| kHR_HmiWdtTimeoutSec | 0x008D | 40142 | INT16 | 1 | UINT16 R/W  0=disabled watchdog sec |
| kHR_HmiHeartbeatAgeSec | 0x008E | 40143 | INT16 | 1 | UINT16 R    seconds since last heartbeat |
| kHR_HmiReserved | 0x008F | 40144 | INT16 | 1 | UINT16 R    reserved |
| kHR_HmiPresetTareHi | 0x0090 | 40145 | UINT16 | 1 | FLOAT32 R/W tare weight kg |
| kHR_HmiPresetTareLo | 0x0091 | 40146 | UINT16 | 1 |  |
| kHR_HmiPresetTargetHi | 0x0092 | 40147 | UINT16 | 1 | FLOAT32 R/W target fill weight kg |
| kHR_HmiPresetTargetLo | 0x0093 | 40148 | UINT16 | 1 |  |
| kHR_HmiPresetRateHi | 0x0094 | 40149 | UINT16 | 1 | FLOAT32 R/W rate per kg (PKR/kg) |
| kHR_HmiPresetRateLo | 0x0095 | 40150 | UINT16 | 1 |  |
| kHR_HmiPresetAmountHi | 0x0096 | 40151 | UINT16 | 1 | FLOAT32 R/W target amount (PKR) |
| kHR_HmiPresetAmountLo | 0x0097 | 40152 | UINT16 | 1 |  |
| kHR_HmiPresetValid | 0x0098 | 40153 | INT16 | 1 | UINT16 R    1=preset validated |
| kHR_HmiPresetErrCode | 0x0099 | 40154 | INT16 | 1 | UINT16 R    kHmiErr_* from last prepare |
| kHR_HmiLastFillResult | 0x009A | 40155 | INT16 | 1 | UINT16 R    kHmiResult_* of last fill |
| kHR_HmiLastFillErrCode | 0x009B | 40156 | INT16 | 1 | UINT16 R    kHmiErr_* of last fill |

## Key Facts for Weintek

- **Driver**: MODBUS RTU (Zero-based Addressing)
- **Address type**: 4x (Holding Registers, FC03)
- **Float32 word order**: High-word-first (ABCD) — reg[n]=high, reg[n+1]=low
- **Baud**: 9600  **Format**: 8N1  **Slave ID**: 1
- **LiveWeight**: 4x address 0, 32-bit Float, High word first
- **DeviceId**: 4x address 24, 16-bit Unsigned, expected 0xA601 (42497)
- **HMI block**: 4x address 128 (0x0080)