#include "ModbusRegisterCache.h"

using namespace ModbusRegisterMap;

// ── Internal helpers ──────────────────────────────────────────────────────────

void ModbusRegisterCache::setFloat(uint16_t* regs, uint16_t addr, float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    regs[addr]     = static_cast<uint16_t>(bits >> 16);
    regs[addr + 1] = static_cast<uint16_t>(bits & 0xFFFF);
}

void ModbusRegisterCache::setUint32(uint16_t* regs, uint16_t addr, uint32_t v) {
    regs[addr]     = static_cast<uint16_t>((v >> 16) & 0xFFFF);
    regs[addr + 1] = static_cast<uint16_t>(v & 0xFFFF);
}

// ── Fast process/IO/comms/alarm update ───────────────────────────────────────
// Covers 0x0000-0x001F and 0x0048-0x0050.
// Deliberately skips RTC (0x0020-0x0027), stats (0x0028-0x0047),
// and resource (0x0051-0x0077) — those have their own update functions.

void ModbusRegisterCache::updateFast(const StatusSnapshot& s, const SettingsStore& settings, bool mqttConnected) {
    // ── Section 1: Process data (0x0000-0x0018) ──────────────────────────────
    setFloat(regs_, kHR_LiveWeightHi,   s.liveWeightKg);
    setFloat(regs_, kHR_TareWeightHi,   s.tareWeightKg);
    setFloat(regs_, kHR_NetWeightHi,    s.netWeightKg);
    setFloat(regs_, kHR_TargetWeightHi, s.targetWeightKg);
    setFloat(regs_, kHR_RatePerKgHi,    s.ratePerKg);
    setFloat(regs_, kHR_TargetAmountHi, s.targetAmount);
    setFloat(regs_, kHR_CurrentAmountHi, s.netWeightKg * s.ratePerKg);

    // Fill state code
    uint16_t fs = 0;
    switch (s.state) {
        case ProcessState::Idle:        fs = kFillState_Idle;        break;
        case ProcessState::Ready:       fs = kFillState_Ready;       break;
        case ProcessState::Validating:  fs = kFillState_Validating;  break;
        case ProcessState::FillingFast: fs = kFillState_Fast;        break;
        case ProcessState::FillingSlow: fs = kFillState_Slow;        break;
        case ProcessState::Settling:    fs = kFillState_Settling;    break;
        case ProcessState::Complete:    fs = kFillState_Complete;    break;
        case ProcessState::Aborted:     fs = kFillState_Aborted;     break;
        case ProcessState::Fault:       fs = kFillState_Fault;       break;
        case ProcessState::Maintenance: fs = kFillState_Maintenance; break;
        default: fs = 0; break;
    }
    regs_[kHR_FillState]       = fs;
    regs_[kHR_EstopOk]         = s.emergencyStopOk  ? 1u : 0u;
    regs_[kHR_CylinderPresent] = s.cylinderPresent  ? 1u : 0u;
    regs_[kHR_NozzleEngaged]   = s.nozzleEngaged    ? 1u : 0u;
    regs_[kHR_WeightStable]    = s.weightStable     ? 1u : 0u;

    // TxnCount and Uptime: cheap inline compute
    const uint32_t uptime = millis() / 1000UL;
    setUint32(regs_, kHR_UptimeHi, uptime);
    // kHR_TxnCountHi/Lo left to updateStats (contains real count from TransactionLog)
    // kHR_Command always reads 0
    regs_[kHR_Command]  = 0;
    regs_[kHR_DeviceId] = 0xA601;

    // ── Section 2: Communications (0x0019-0x001F) ────────────────────────────
    const ModbusRtuSettings rtu = settings.rtuSnapshot();
    regs_[kHR_RtuSlaveAddr] = rtu.slaveAddress;
    setUint32(regs_, kHR_RtuBaudHi, rtu.baudRate);
    regs_[kHR_RtuParity]   = rtu.parity;
    regs_[kHR_RtuStopBits] = rtu.stopBits;
    regs_[kHR_TcpPort]     = 502;
    regs_[kHR_MqttConnected] = mqttConnected ? 1u : 0u;

    // ── Section 6: Diagnostics/alarm (0x0048-0x0050) ─────────────────────────
    // Alarm code logic (mirrors ModbusRegisterMap.cpp)
    const bool readinessRequired =
        s.state == ProcessState::Idle      ||
        s.state == ProcessState::Ready     ||
        s.state == ProcessState::Validating||
        s.state == ProcessState::FillingFast||
        s.state == ProcessState::FillingSlow||
        s.state == ProcessState::Settling;

    uint16_t alarmCode = kAlarm_None;
    if (!s.emergencyStopOk || s.lastReasonCode == "emergency_stop")
        alarmCode = kAlarm_EmergencyStop;
    else if (s.lastReasonCode == "nozzle_disengaged" || (readinessRequired && !s.nozzleEngaged))
        alarmCode = kAlarm_NozzleDisengaged;
    else if (readinessRequired && !s.cylinderPresent)
        alarmCode = kAlarm_CylinderMissing;
    else if (s.lastReasonCode == "scale_read_error" || s.weightReadError)
        alarmCode = kAlarm_ScaleReadError;
    else if (readinessRequired && !s.weightStable && (s.state == ProcessState::Idle || s.state == ProcessState::Ready))
        alarmCode = kAlarm_ScaleNotStable;
    else if (readinessRequired && !s.calibrationValid)
        alarmCode = kAlarm_ScaleNotCalibrated;
    else if (s.lastReasonCode == "overfill")            alarmCode = kAlarm_Overfill;
    else if (s.lastReasonCode == "fill_timeout")        alarmCode = kAlarm_FillTimeout;
    else if (s.lastReasonCode == "no_flow")             alarmCode = kAlarm_NoFlow;
    else if (s.lastReasonCode == "transaction_log_failed") alarmCode = kAlarm_TransactionLog;
    else if (s.lastReasonCode == "operator_stop" ||
             s.lastReasonCode == "serial_stop"   ||
             s.lastReasonCode == "modbus_stop")         alarmCode = kAlarm_OperatorStop;
    else if (s.state == ProcessState::Fault)            alarmCode = kAlarm_ActiveFault;

    uint16_t severity = 0;
    if (alarmCode != kAlarm_None) {
        if (s.state == ProcessState::Fault ||
            alarmCode == kAlarm_EmergencyStop ||
            alarmCode == kAlarm_Overfill ||
            alarmCode == kAlarm_NoFlow ||
            alarmCode == kAlarm_FillTimeout) severity = 3;
        else if (s.state == ProcessState::Complete) severity = 1;
        else severity = 2;
    }

    uint16_t alarmSrc = 0;
    if (alarmCode != kAlarm_None) {
        if (alarmCode == kAlarm_EmergencyStop  ||
            alarmCode == kAlarm_NozzleDisengaged||
            alarmCode == kAlarm_CylinderMissing) alarmSrc = 1;
        else if (alarmCode == kAlarm_ScaleReadError    ||
                 alarmCode == kAlarm_ScaleNotStable    ||
                 alarmCode == kAlarm_ScaleNotCalibrated) alarmSrc = 2;
        else if (alarmCode == kAlarm_OperatorStop)       alarmSrc = 4;
        else alarmSrc = 3;
    }

    uint16_t readinessMask = 0;
    if (s.emergencyStopOk)  readinessMask |= (1u << 0);
    if (s.cylinderPresent)  readinessMask |= (1u << 1);
    if (s.nozzleEngaged)    readinessMask |= (1u << 2);
    if (s.weightStable)     readinessMask |= (1u << 3);
    if (s.calibrationValid) readinessMask |= (1u << 4);
    if (s.weightInitialized && !s.weightReadError) readinessMask |= (1u << 5);

    uint16_t blockerMask = 0;
    if (s.state == ProcessState::Fault) blockerMask |= (1u << 0);
    if (!s.emergencyStopOk)             blockerMask |= (1u << 1);
    if (!s.cylinderPresent)             blockerMask |= (1u << 2);
    if (!s.nozzleEngaged)               blockerMask |= (1u << 3);
    if (!s.weightInitialized)           blockerMask |= (1u << 4);
    if (s.weightReadError)              blockerMask |= (1u << 5);
    if (!s.weightStable)                blockerMask |= (1u << 6);
    if (!s.calibrationValid)            blockerMask |= (1u << 7);
    if (s.simulationActive)             blockerMask |= (1u << 8);

    regs_[kHR_AlarmCode]       = alarmCode;
    regs_[kHR_AlarmSeverity]   = severity;
    regs_[kHR_ReadinessMask]   = readinessMask;
    regs_[kHR_BlockerMask]     = blockerMask;
    regs_[kHR_ScaleInitialized]= s.weightInitialized ? 1u : 0u;
    regs_[kHR_ScaleReadError]  = s.weightReadError   ? 1u : 0u;
    regs_[kHR_CalibrationValid]= s.calibrationValid  ? 1u : 0u;
    regs_[kHR_SimulationActive]= s.simulationActive  ? 1u : 0u;
    regs_[kHR_AlarmSource]     = alarmSrc;

    // ── Coils ─────────────────────────────────────────────────────────────────
    const bool fillActive = s.state == ProcessState::FillingFast ||
                            s.state == ProcessState::FillingSlow ||
                            s.state == ProcessState::Settling    ||
                            s.state == ProcessState::Validating;
    coils_[kCoil_EstopOk]       = s.emergencyStopOk ? 1u : 0u;
    coils_[kCoil_CylinderPres]  = s.cylinderPresent ? 1u : 0u;
    coils_[kCoil_NozzleEngaged] = s.nozzleEngaged   ? 1u : 0u;
    coils_[kCoil_WeightStable]  = s.weightStable    ? 1u : 0u;
    coils_[kCoil_FillActive]    = fillActive        ? 1u : 0u;
    for (uint8_t i = 0; i < BoardConfig::kRelayCount && i < 6; i++)
        coils_[kCoil_Relay1 + i] = s.relays[i] ? 1u : 0u;

    // ── Discrete inputs ───────────────────────────────────────────────────────
    for (uint8_t i = 0; i < BoardConfig::kInputCount && i < kDI_Count; i++)
        di_[i] = s.inputs[i] ? 1u : 0u;
}

// ── RTC update (0x0020-0x0027) ────────────────────────────────────────────────

void ModbusRegisterCache::updateRtc(const RtcTime& t) {
    regs_[kHR_RtcYear]   = t.year;
    regs_[kHR_RtcMonth]  = t.month;
    regs_[kHR_RtcDay]    = t.date;
    regs_[kHR_RtcHour]   = t.hour;
    regs_[kHR_RtcMinute] = t.minute;
    regs_[kHR_RtcSecond] = t.second;

    uint32_t unix = 0;
    if (t.year >= 2020) {
        struct tm tm = {};
        tm.tm_year = t.year - 1900;
        tm.tm_mon  = t.month - 1;
        tm.tm_mday = t.date;
        tm.tm_hour = t.hour;
        tm.tm_min  = t.minute;
        tm.tm_sec  = t.second;
        unix = static_cast<uint32_t>(mktime(&tm));
    }
    setUint32(regs_, kHR_RtcUnixHi, unix);
}

// ── Statistics update (0x0028-0x0047) ────────────────────────────────────────

void ModbusRegisterCache::updateStats(const TransactionLog& txnLog) {
    // TxnCount updated here so it is consistent with all-time stats
    setUint32(regs_, kHR_TxnCountHi, txnLog.totalCount());

    setUint32(regs_, kHR_StatAllCompHi, txnLog.completedCount());
    setUint32(regs_, kHR_StatAllFailHi, txnLog.abortedCount() + txnLog.faultCount());
    setFloat(regs_, kHR_StatAllKgHi,  txnLog.allKgTotal());
    setFloat(regs_, kHR_StatAllAmtHi, txnLog.allAmountTotal());

    // computeStats() has its own 30 s TTL cache — safe to call here
    const TxnStatsSnapshot s = txnLog.computeStats();

    regs_[kHR_StatTodayComp] = static_cast<uint16_t>(s.todayCompleted);
    regs_[kHR_StatTodayFail] = static_cast<uint16_t>(s.todayFailed);
    setFloat(regs_, kHR_StatTodayKgHi,  s.todayKg);
    setFloat(regs_, kHR_StatTodayAmtHi, s.todayAmount);

    regs_[kHR_StatWeekComp] = static_cast<uint16_t>(s.weekCompleted);
    regs_[kHR_StatWeekFail] = static_cast<uint16_t>(s.weekFailed);
    setFloat(regs_, kHR_StatWeekKgHi,  s.weekKg);
    setFloat(regs_, kHR_StatWeekAmtHi, s.weekAmount);

    regs_[kHR_StatMonthComp] = static_cast<uint16_t>(s.monthCompleted);
    regs_[kHR_StatMonthFail] = static_cast<uint16_t>(s.monthFailed);
    setFloat(regs_, kHR_StatMonthKgHi,  s.monthKg);
    setFloat(regs_, kHR_StatMonthAmtHi, s.monthAmount);

    regs_[kHR_StatYearComp] = static_cast<uint16_t>(s.yearCompleted);
    regs_[kHR_StatYearFail] = static_cast<uint16_t>(s.yearFailed);
    setFloat(regs_, kHR_StatYearKgHi,  s.yearKg);
    setFloat(regs_, kHR_StatYearAmtHi, s.yearAmount);
}

// ── Resource monitor update (0x0051-0x0077) ───────────────────────────────────

void ModbusRegisterCache::updateResource(const ResourceSnapshot& r) {
    setFloat(regs_,  kHR_ApplicationLoadHi, r.applicationLoadPercent);
    setFloat(regs_,  kHR_LoopAvgMsHi,       r.mainLoopAverageMs);
    setFloat(regs_,  kHR_LoopMaxMsHi,       r.mainLoopMaximumMs);
    setUint32(regs_, kHR_HeapTotalHi,        r.heapTotalBytes);
    setUint32(regs_, kHR_HeapFreeHi,         r.heapFreeBytes);
    setUint32(regs_, kHR_HeapMinFreeHi,      r.heapMinFreeBytes);
    setFloat(regs_,  kHR_HeapFreePctHi,      r.heapFreePercent);
    setUint32(regs_, kHR_PsramTotalHi,       r.psramTotalBytes);
    setUint32(regs_, kHR_PsramFreeHi,        r.psramFreeBytes);
    setFloat(regs_,  kHR_PsramFreePctHi,     r.psramFreePercent);
    setUint32(regs_, kHR_FlashSizeHi,        r.flashSizeBytes);
    setUint32(regs_, kHR_SketchSizeHi,       r.sketchSizeBytes);
    setUint32(regs_, kHR_FreeSketchHi,       r.freeSketchBytes);
    setFloat(regs_,  kHR_ChipTempHi,         r.chipTemperatureC);
    regs_[kHR_WifiRssi]        = static_cast<uint16_t>(r.wifiRssiDbm);
    regs_[kHR_WifiStatus]      = r.wifiStatus;
    regs_[kHR_MqttClientState] = static_cast<uint16_t>(r.mqttClientState);
    regs_[kHR_LastResetReason] = r.lastResetReason;
    regs_[kHR_FirmwareBuildMode] = r.firmwareBuildMode;
    setUint32(regs_, kHR_RtuReqCountHi, r.modbusRtuRequestCount);
    setUint32(regs_, kHR_RtuErrCountHi, r.modbusRtuErrorCount);
    setUint32(regs_, kHR_HeartbeatHi,   r.heartbeatCounter);
}

// ── Hot-path read functions ───────────────────────────────────────────────────

void ModbusRegisterCache::readBlock(uint16_t startAddr, uint16_t qty, uint8_t* dest) const {
    for (uint16_t i = 0; i < qty; i++) {
        const uint16_t v = regs_[startAddr + i];
        dest[i * 2]     = static_cast<uint8_t>(v >> 8);
        dest[i * 2 + 1] = static_cast<uint8_t>(v & 0xFF);
    }
}

uint16_t ModbusRegisterCache::readReg(uint16_t addr) const {
    if (addr >= kHR_Count) return 0;
    return regs_[addr];
}

uint8_t ModbusRegisterCache::readCoil(uint16_t addr) const {
    if (addr >= kCoil_Count) return 0;
    return coils_[addr];
}

uint8_t ModbusRegisterCache::readDI(uint16_t addr) const {
    if (addr >= kDI_Count) return 0;
    return di_[addr];
}
