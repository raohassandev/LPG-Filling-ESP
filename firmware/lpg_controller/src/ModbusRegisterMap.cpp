#include "ModbusRegisterMap.h"
#include "ResourceMonitor.h"

// Production default is read-only. Enable writes explicitly with either:
//   -DLPG_MODBUS_WRITES_ENABLED=1
//   -DLPG_PROTOTYPE_BUILD=1
// Prototype builds can then write target/rate/amount and command registers.
#ifndef LPG_MODBUS_WRITES_ENABLED
  #if defined(LPG_PROTOTYPE_BUILD) && LPG_PROTOTYPE_BUILD
    #define LPG_MODBUS_WRITES_ENABLED 1
  #else
    #define LPG_MODBUS_WRITES_ENABLED 0
  #endif
#endif

namespace {

void floatToRegs(float f, uint16_t& hi, uint16_t& lo) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    hi = static_cast<uint16_t>(bits >> 16);
    lo = static_cast<uint16_t>(bits & 0xFFFF);
}

void uint32ToRegs(uint32_t value, uint16_t& hi, uint16_t& lo) {
    hi = static_cast<uint16_t>((value >> 16) & 0xFFFF);
    lo = static_cast<uint16_t>(value & 0xFFFF);
}

float regsToFloat(uint16_t hi, uint16_t lo) {
    const uint32_t bits = (static_cast<uint32_t>(hi) << 16) | lo;
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

uint16_t fillStateCode(ProcessState s) {
    using namespace ModbusRegisterMap;
    switch (s) {
        case ProcessState::Idle:        return kFillState_Idle;
        case ProcessState::Ready:       return kFillState_Ready;
        case ProcessState::Validating:  return kFillState_Validating;
        case ProcessState::FillingFast: return kFillState_Fast;
        case ProcessState::FillingSlow: return kFillState_Slow;
        case ProcessState::Settling:    return kFillState_Settling;
        case ProcessState::Complete:    return kFillState_Complete;
        case ProcessState::Aborted:     return kFillState_Aborted;
        case ProcessState::Fault:       return kFillState_Fault;
        case ProcessState::Maintenance: return kFillState_Maintenance;
        default:                        return 0;
    }
}

bool isFillActive(ProcessState s) {
    return s == ProcessState::FillingFast ||
           s == ProcessState::FillingSlow ||
           s == ProcessState::Settling    ||
           s == ProcessState::Validating;
}

// Day-of-week from Y/M/D (Tomohiko Sakamoto's algorithm), returns 0=Sun…6=Sat
uint16_t alarmCode(const StatusSnapshot& s) {
    using namespace ModbusRegisterMap;
    const bool readinessRequired =
        s.state == ProcessState::Idle ||
        s.state == ProcessState::Ready ||
        s.state == ProcessState::Validating ||
        s.state == ProcessState::FillingFast ||
        s.state == ProcessState::FillingSlow ||
        s.state == ProcessState::Settling;
    if (!s.emergencyStopOk || s.lastReasonCode == "emergency_stop") return kAlarm_EmergencyStop;
    if (s.lastReasonCode == "nozzle_disengaged" || (readinessRequired && !s.nozzleEngaged)) return kAlarm_NozzleDisengaged;
    if (readinessRequired && !s.cylinderPresent) return kAlarm_CylinderMissing;
    if (s.lastReasonCode == "scale_read_error" || s.weightReadError) return kAlarm_ScaleReadError;
    if (readinessRequired && !s.weightStable && (s.state == ProcessState::Idle || s.state == ProcessState::Ready)) return kAlarm_ScaleNotStable;
    if (readinessRequired && !s.calibrationValid) return kAlarm_ScaleNotCalibrated;
    if (s.lastReasonCode == "overfill") return kAlarm_Overfill;
    if (s.lastReasonCode == "fill_timeout") return kAlarm_FillTimeout;
    if (s.lastReasonCode == "no_flow") return kAlarm_NoFlow;
    if (s.lastReasonCode == "transaction_log_failed") return kAlarm_TransactionLog;
    if (s.lastReasonCode == "operator_stop" || s.lastReasonCode == "serial_stop" || s.lastReasonCode == "modbus_stop") return kAlarm_OperatorStop;
    if (s.state == ProcessState::Fault) return kAlarm_ActiveFault;
    return kAlarm_None;
}

uint16_t alarmSeverity(const StatusSnapshot& s) {
    const uint16_t code = alarmCode(s);
    if (code == ModbusRegisterMap::kAlarm_None) return 0;
    if (s.state == ProcessState::Fault || code == ModbusRegisterMap::kAlarm_EmergencyStop ||
        code == ModbusRegisterMap::kAlarm_Overfill || code == ModbusRegisterMap::kAlarm_NoFlow ||
        code == ModbusRegisterMap::kAlarm_FillTimeout) return 3;
    if (s.state == ProcessState::Complete) return 1;
    return 2;
}

uint16_t readinessMask(const StatusSnapshot& s) {
    uint16_t m = 0;
    if (s.emergencyStopOk)    m |= (1u << 0);
    if (s.cylinderPresent)    m |= (1u << 1);
    if (s.nozzleEngaged)      m |= (1u << 2);
    if (s.weightStable)       m |= (1u << 3);
    if (s.calibrationValid)   m |= (1u << 4);
    if (s.weightInitialized && !s.weightReadError) m |= (1u << 5);
    return m;
}

uint16_t blockerMask(const StatusSnapshot& s) {
    uint16_t m = 0;
    if (s.state == ProcessState::Fault) m |= (1u << 0);
    if (!s.emergencyStopOk)             m |= (1u << 1);
    if (!s.cylinderPresent)             m |= (1u << 2);
    if (!s.nozzleEngaged)               m |= (1u << 3);
    if (!s.weightInitialized)           m |= (1u << 4);
    if (s.weightReadError)              m |= (1u << 5);
    if (!s.weightStable)                m |= (1u << 6);
    if (!s.calibrationValid)            m |= (1u << 7);
    if (s.simulationActive)             m |= (1u << 8);
    return m;
}

uint16_t alarmSource(const StatusSnapshot& s) {
    const uint16_t code = alarmCode(s);
    if (code == ModbusRegisterMap::kAlarm_None) return 0;
    if (code == ModbusRegisterMap::kAlarm_EmergencyStop ||
        code == ModbusRegisterMap::kAlarm_NozzleDisengaged ||
        code == ModbusRegisterMap::kAlarm_CylinderMissing) return 1;
    if (code == ModbusRegisterMap::kAlarm_ScaleReadError ||
        code == ModbusRegisterMap::kAlarm_ScaleNotStable ||
        code == ModbusRegisterMap::kAlarm_ScaleNotCalibrated) return 2;
    if (code == ModbusRegisterMap::kAlarm_OperatorStop) return 4;
    return 3;
}

uint8_t dayOfWeek(uint16_t y, uint8_t m, uint8_t d) {
    static const int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    if (m < 3) y--;
    return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

}  // namespace

// ── Holding Register read ─────────────────────────────────────────────────────
uint16_t ModbusRegisterMap::readHR(uint16_t addr, const StatusSnapshot& status,
                                    const TransactionLog& txnLog,
                                    const SettingsStore& settings,
                                    const RtcTime& rtc,
                                    bool mqttConnected) {
    uint16_t hi, lo;

    switch (addr) {
        // ── Section 1: Process data ──────────────────────────────────────────
        case kHR_LiveWeightHi:    floatToRegs(status.liveWeightKg,   hi, lo); return hi;
        case kHR_LiveWeightLo:    floatToRegs(status.liveWeightKg,   hi, lo); return lo;
        case kHR_TareWeightHi:    floatToRegs(status.tareWeightKg,   hi, lo); return hi;
        case kHR_TareWeightLo:    floatToRegs(status.tareWeightKg,   hi, lo); return lo;
        case kHR_NetWeightHi:     floatToRegs(status.netWeightKg,    hi, lo); return hi;
        case kHR_NetWeightLo:     floatToRegs(status.netWeightKg,    hi, lo); return lo;
        case kHR_TargetWeightHi:  floatToRegs(status.targetWeightKg, hi, lo); return hi;
        case kHR_TargetWeightLo:  floatToRegs(status.targetWeightKg, hi, lo); return lo;
        case kHR_RatePerKgHi:     floatToRegs(status.ratePerKg,      hi, lo); return hi;
        case kHR_RatePerKgLo:     floatToRegs(status.ratePerKg,      hi, lo); return lo;
        case kHR_TargetAmountHi:  floatToRegs(status.targetAmount,   hi, lo); return hi;
        case kHR_TargetAmountLo:  floatToRegs(status.targetAmount,   hi, lo); return lo;
        case kHR_CurrentAmountHi: { const float c = status.netWeightKg * status.ratePerKg; floatToRegs(c, hi, lo); return hi; }
        case kHR_CurrentAmountLo: { const float c = status.netWeightKg * status.ratePerKg; floatToRegs(c, hi, lo); return lo; }
        case kHR_FillState:       return fillStateCode(status.state);
        case kHR_EstopOk:         return status.emergencyStopOk  ? 1 : 0;
        case kHR_CylinderPresent: return status.cylinderPresent  ? 1 : 0;
        case kHR_NozzleEngaged:   return status.nozzleEngaged    ? 1 : 0;
        case kHR_WeightStable:    return status.weightStable     ? 1 : 0;
        case kHR_TxnCountHi: return static_cast<uint16_t>((txnLog.totalCount() >> 16) & 0xFFFF);
        case kHR_TxnCountLo: return static_cast<uint16_t>( txnLog.totalCount()        & 0xFFFF);
        case kHR_UptimeHi:   return static_cast<uint16_t>(((millis() / 1000UL) >> 16) & 0xFFFF);
        case kHR_UptimeLo:   return static_cast<uint16_t>( (millis() / 1000UL)        & 0xFFFF);
        case kHR_Command:    return 0;
        case kHR_DeviceId:   return 0xA601;

        // ── Section 2: Communications ────────────────────────────────────────
        case kHR_RtuSlaveAddr: return settings.rtuSnapshot().slaveAddress;
        case kHR_RtuBaudHi:    return static_cast<uint16_t>((settings.rtuSnapshot().baudRate >> 16) & 0xFFFF);
        case kHR_RtuBaudLo:    return static_cast<uint16_t>( settings.rtuSnapshot().baudRate        & 0xFFFF);
        case kHR_RtuParity:    return settings.rtuSnapshot().parity;
        case kHR_RtuStopBits:  return settings.rtuSnapshot().stopBits;
        case kHR_TcpPort:      return 502;
        case kHR_MqttConnected: return mqttConnected ? 1 : 0;

        // ── Section 3: RTC / Clock ───────────────────────────────────────────
        case kHR_RtcYear:   return rtc.year;
        case kHR_RtcMonth:  return rtc.month;
        case kHR_RtcDay:    return rtc.date;
        case kHR_RtcHour:   return rtc.hour;
        case kHR_RtcMinute: return rtc.minute;
        case kHR_RtcSecond: return rtc.second;
        case kHR_RtcUnixHi: {
            // Convert RTC fields to Unix timestamp and return Hi word
            if (rtc.year < 2020) return 0;
            struct tm t = {};
            t.tm_year = rtc.year - 1900; t.tm_mon = rtc.month - 1; t.tm_mday = rtc.date;
            t.tm_hour = rtc.hour; t.tm_min = rtc.minute; t.tm_sec = rtc.second;
            const uint32_t unix = (uint32_t)mktime(&t);
            return static_cast<uint16_t>((unix >> 16) & 0xFFFF);
        }
        case kHR_RtcUnixLo: {
            if (rtc.year < 2020) return 0;
            struct tm t = {};
            t.tm_year = rtc.year - 1900; t.tm_mon = rtc.month - 1; t.tm_mday = rtc.date;
            t.tm_hour = rtc.hour; t.tm_min = rtc.minute; t.tm_sec = rtc.second;
            const uint32_t unix = (uint32_t)mktime(&t);
            return static_cast<uint16_t>(unix & 0xFFFF);
        }

        // ── Section 4: All-time statistics ───────────────────────────────────
        case kHR_StatAllCompHi: return static_cast<uint16_t>((txnLog.completedCount() >> 16) & 0xFFFF);
        case kHR_StatAllCompLo: return static_cast<uint16_t>( txnLog.completedCount()        & 0xFFFF);
        case kHR_StatAllFailHi: {
            const uint32_t f = txnLog.abortedCount() + txnLog.faultCount();
            return static_cast<uint16_t>((f >> 16) & 0xFFFF);
        }
        case kHR_StatAllFailLo: {
            const uint32_t f = txnLog.abortedCount() + txnLog.faultCount();
            return static_cast<uint16_t>(f & 0xFFFF);
        }
        case kHR_StatAllKgHi:  floatToRegs(txnLog.allKgTotal(),     hi, lo); return hi;
        case kHR_StatAllKgLo:  floatToRegs(txnLog.allKgTotal(),     hi, lo); return lo;
        case kHR_StatAllAmtHi: floatToRegs(txnLog.allAmountTotal(), hi, lo); return hi;
        case kHR_StatAllAmtLo: floatToRegs(txnLog.allAmountTotal(), hi, lo); return lo;

        // ── Section 5: Period statistics (computeStats is cached) ────────────
        case kHR_StatTodayComp:  return static_cast<uint16_t>(txnLog.computeStats().todayCompleted);
        case kHR_StatTodayFail:  return static_cast<uint16_t>(txnLog.computeStats().todayFailed);
        case kHR_StatTodayKgHi:  floatToRegs(txnLog.computeStats().todayKg,     hi, lo); return hi;
        case kHR_StatTodayKgLo:  floatToRegs(txnLog.computeStats().todayKg,     hi, lo); return lo;
        case kHR_StatTodayAmtHi: floatToRegs(txnLog.computeStats().todayAmount, hi, lo); return hi;
        case kHR_StatTodayAmtLo: floatToRegs(txnLog.computeStats().todayAmount, hi, lo); return lo;

        case kHR_StatWeekComp:   return static_cast<uint16_t>(txnLog.computeStats().weekCompleted);
        case kHR_StatWeekFail:   return static_cast<uint16_t>(txnLog.computeStats().weekFailed);
        case kHR_StatWeekKgHi:   floatToRegs(txnLog.computeStats().weekKg,     hi, lo); return hi;
        case kHR_StatWeekKgLo:   floatToRegs(txnLog.computeStats().weekKg,     hi, lo); return lo;
        case kHR_StatWeekAmtHi:  floatToRegs(txnLog.computeStats().weekAmount, hi, lo); return hi;
        case kHR_StatWeekAmtLo:  floatToRegs(txnLog.computeStats().weekAmount, hi, lo); return lo;

        case kHR_StatMonthComp:  return static_cast<uint16_t>(txnLog.computeStats().monthCompleted);
        case kHR_StatMonthFail:  return static_cast<uint16_t>(txnLog.computeStats().monthFailed);
        case kHR_StatMonthKgHi:  floatToRegs(txnLog.computeStats().monthKg,     hi, lo); return hi;
        case kHR_StatMonthKgLo:  floatToRegs(txnLog.computeStats().monthKg,     hi, lo); return lo;
        case kHR_StatMonthAmtHi: floatToRegs(txnLog.computeStats().monthAmount, hi, lo); return hi;
        case kHR_StatMonthAmtLo: floatToRegs(txnLog.computeStats().monthAmount, hi, lo); return lo;

        case kHR_StatYearComp:   return static_cast<uint16_t>(txnLog.computeStats().yearCompleted);
        case kHR_StatYearFail:   return static_cast<uint16_t>(txnLog.computeStats().yearFailed);
        case kHR_StatYearKgHi:   floatToRegs(txnLog.computeStats().yearKg,     hi, lo); return hi;
        case kHR_StatYearKgLo:   floatToRegs(txnLog.computeStats().yearKg,     hi, lo); return lo;
        case kHR_StatYearAmtHi:  floatToRegs(txnLog.computeStats().yearAmount, hi, lo); return hi;
        case kHR_StatYearAmtLo:  floatToRegs(txnLog.computeStats().yearAmount, hi, lo); return lo;

        case kHR_AlarmCode:        return alarmCode(status);
        case kHR_AlarmSeverity:    return alarmSeverity(status);
        case kHR_ReadinessMask:    return readinessMask(status);
        case kHR_BlockerMask:      return blockerMask(status);
        case kHR_ScaleInitialized: return status.weightInitialized ? 1 : 0;
        case kHR_ScaleReadError:   return status.weightReadError ? 1 : 0;
        case kHR_CalibrationValid: return status.calibrationValid ? 1 : 0;
        case kHR_SimulationActive: return status.simulationActive ? 1 : 0;
        case kHR_AlarmSource:      return alarmSource(status);

        case kHR_ApplicationLoadHi: { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); floatToRegs(r.applicationLoadPercent, hi, lo); return hi; }
        case kHR_ApplicationLoadLo: { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); floatToRegs(r.applicationLoadPercent, hi, lo); return lo; }
        case kHR_LoopAvgMsHi:       { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); floatToRegs(r.mainLoopAverageMs, hi, lo); return hi; }
        case kHR_LoopAvgMsLo:       { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); floatToRegs(r.mainLoopAverageMs, hi, lo); return lo; }
        case kHR_LoopMaxMsHi:       { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); floatToRegs(r.mainLoopMaximumMs, hi, lo); return hi; }
        case kHR_LoopMaxMsLo:       { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); floatToRegs(r.mainLoopMaximumMs, hi, lo); return lo; }
        case kHR_HeapTotalHi:       { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.heapTotalBytes, hi, lo); return hi; }
        case kHR_HeapTotalLo:       { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.heapTotalBytes, hi, lo); return lo; }
        case kHR_HeapFreeHi:        { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.heapFreeBytes, hi, lo); return hi; }
        case kHR_HeapFreeLo:        { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.heapFreeBytes, hi, lo); return lo; }
        case kHR_HeapMinFreeHi:     { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.heapMinFreeBytes, hi, lo); return hi; }
        case kHR_HeapMinFreeLo:     { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.heapMinFreeBytes, hi, lo); return lo; }
        case kHR_HeapFreePctHi:     { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); floatToRegs(r.heapFreePercent, hi, lo); return hi; }
        case kHR_HeapFreePctLo:     { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); floatToRegs(r.heapFreePercent, hi, lo); return lo; }
        case kHR_PsramTotalHi:      { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.psramTotalBytes, hi, lo); return hi; }
        case kHR_PsramTotalLo:      { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.psramTotalBytes, hi, lo); return lo; }
        case kHR_PsramFreeHi:       { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.psramFreeBytes, hi, lo); return hi; }
        case kHR_PsramFreeLo:       { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.psramFreeBytes, hi, lo); return lo; }
        case kHR_PsramFreePctHi:    { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); floatToRegs(r.psramFreePercent, hi, lo); return hi; }
        case kHR_PsramFreePctLo:    { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); floatToRegs(r.psramFreePercent, hi, lo); return lo; }
        case kHR_FlashSizeHi:       { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.flashSizeBytes, hi, lo); return hi; }
        case kHR_FlashSizeLo:       { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.flashSizeBytes, hi, lo); return lo; }
        case kHR_SketchSizeHi:      { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.sketchSizeBytes, hi, lo); return hi; }
        case kHR_SketchSizeLo:      { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.sketchSizeBytes, hi, lo); return lo; }
        case kHR_FreeSketchHi:      { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.freeSketchBytes, hi, lo); return hi; }
        case kHR_FreeSketchLo:      { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.freeSketchBytes, hi, lo); return lo; }
        case kHR_ChipTempHi:        { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); floatToRegs(r.chipTemperatureC, hi, lo); return hi; }
        case kHR_ChipTempLo:        { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); floatToRegs(r.chipTemperatureC, hi, lo); return lo; }
        case kHR_WifiRssi:          return static_cast<uint16_t>(ResourceMonitor::instance().snapshot().wifiRssiDbm);
        case kHR_WifiStatus:        return ResourceMonitor::instance().snapshot().wifiStatus;
        case kHR_MqttClientState:   return static_cast<uint16_t>(ResourceMonitor::instance().snapshot().mqttClientState);
        case kHR_LastResetReason:   return ResourceMonitor::instance().snapshot().lastResetReason;
        case kHR_FirmwareBuildMode: return ResourceMonitor::instance().snapshot().firmwareBuildMode;
        case kHR_RtuReqCountHi:     { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.modbusRtuRequestCount, hi, lo); return hi; }
        case kHR_RtuReqCountLo:     { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.modbusRtuRequestCount, hi, lo); return lo; }
        case kHR_RtuErrCountHi:     { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.modbusRtuErrorCount, hi, lo); return hi; }
        case kHR_RtuErrCountLo:     { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.modbusRtuErrorCount, hi, lo); return lo; }
        case kHR_HeartbeatHi:       { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.heartbeatCounter, hi, lo); return hi; }
        case kHR_HeartbeatLo:       { const ResourceSnapshot r = ResourceMonitor::instance().snapshot(); uint32ToRegs(r.heartbeatCounter, hi, lo); return lo; }

        default: return 0;
    }
}

// ── Modbus writes enabled flag ────────────────────────────────────────────────
bool ModbusRegisterMap::modbusWritesEnabled() {
    return LPG_MODBUS_WRITES_ENABLED != 0;
}

// ── Holding Register write ────────────────────────────────────────────────────
bool ModbusRegisterMap::writeHR(uint16_t addr, uint16_t value,
                                 StatusStore& statusStore, SettingsStore& settingsStore,
                                 FillController& fillController, RtcService& rtcService) {
#if LPG_MODBUS_WRITES_ENABLED == 0
    // Writes disabled in production. Define LPG_MODBUS_WRITES_ENABLED=1 to enable.
    (void)addr; (void)value; (void)statusStore; (void)settingsStore;
    (void)fillController; (void)rtcService;
    ResourceMonitor::instance().recordWriteFail("writes_disabled_production_build");
    return false;
#else
    static uint16_t sHi_TareWeight   = 0;
    static uint16_t sHi_TargetWeight = 0;
    static uint16_t sHi_RatePerKg    = 0;
    static uint16_t sHi_TargetAmount = 0;
    static uint32_t sRtuBaudHi       = 0;
    static uint16_t sRtcYear = 0, sRtcMonth = 0, sRtcDay = 0, sRtcHour = 0, sRtcMinute = 0;
    static uint16_t sRtcUnixHi = 0;

    const StatusSnapshot snap = statusStore.snapshot();

    switch (addr) {

        // ── Tare Weight ──────────────────────────────────────────────────────
        case kHR_TareWeightHi: sHi_TareWeight = value; return true;
        case kHR_TareWeightLo: {
            const float kg = regsToFloat(sHi_TareWeight, value);
            if (kg < 0.0f || kg > 500.0f) {
                ResourceMonitor::instance().recordWriteFail("tare_range");
                return false;
            }
            statusStore.setTareWeight(kg);
            return true;
        }

        // ── Target Weight ────────────────────────────────────────────────────
        case kHR_TargetWeightHi: sHi_TargetWeight = value; return true;
        case kHR_TargetWeightLo: {
            const float kg = regsToFloat(sHi_TargetWeight, value);
            if (kg < 0.0f || kg > 500.0f) {
                ResourceMonitor::instance().recordWriteFail("tgt_range");
                return false;
            }
            statusStore.setTargets(kg, snap.targetAmount, snap.ratePerKg);
            return true;
        }

        // ── Rate Per kg ──────────────────────────────────────────────────────
        case kHR_RatePerKgHi: sHi_RatePerKg = value; return true;
        case kHR_RatePerKgLo: {
            const float rate = regsToFloat(sHi_RatePerKg, value);
            if (rate <= 0.0f || rate > 100000.0f) {
                ResourceMonitor::instance().recordWriteFail("rate_range");
                return false;
            }
            settingsStore.setRatePerKg(rate);
            statusStore.setTargets(snap.targetWeightKg, snap.targetAmount, rate);
            return true;
        }

        // ── Target Amount ────────────────────────────────────────────────────
        case kHR_TargetAmountHi: sHi_TargetAmount = value; return true;
        case kHR_TargetAmountLo: {
            const float amount = regsToFloat(sHi_TargetAmount, value);
            if (amount < 0.0f) {
                ResourceMonitor::instance().recordWriteFail("amt_neg");
                return false;
            }
            statusStore.setTargets(snap.targetWeightKg, amount, snap.ratePerKg);
            return true;
        }

        // ── Command ──────────────────────────────────────────────────────────
        case kHR_Command: {
            String reason;
            switch (value) {
                case 1: return fillController.startFill(snap.targetWeightKg, snap.ratePerKg, snap.targetAmount, reason, "modbus");
                case 2: return fillController.stopFill("modbus_stop");
                case 3: return fillController.resetToIdle(reason);
                case 4: statusStore.setTareWeight(snap.liveWeightKg); return true;
                default:
                    ResourceMonitor::instance().recordWriteFail("cmd_unk");
                    return false;
            }
        }

        // ── RTU Comms parameters (persist, RTU service picks up on next begin/restart) ──
        case kHR_RtuSlaveAddr: {
            if (value < 1 || value > 247) return false;
            ModbusRtuSettings rtu = settingsStore.rtuSnapshot();
            rtu.slaveAddress = static_cast<uint8_t>(value);
            return settingsStore.setModbusRtu(rtu);
        }
        case kHR_RtuBaudHi: { sRtuBaudHi = value; return true; }
        case kHR_RtuBaudLo: {
            const uint32_t baud = ((uint32_t)sRtuBaudHi << 16) | value;
            const uint32_t valid[] = {1200,2400,4800,9600,19200,38400,57600,115200};
            bool ok = false;
            for (auto v : valid) { if (baud == v) { ok = true; break; } }
            if (!ok) return false;
            ModbusRtuSettings rtu = settingsStore.rtuSnapshot();
            rtu.baudRate = baud;
            return settingsStore.setModbusRtu(rtu);
        }
        case kHR_RtuParity: {
            if (value > 2) return false;
            ModbusRtuSettings rtu = settingsStore.rtuSnapshot();
            rtu.parity = static_cast<uint8_t>(value);
            return settingsStore.setModbusRtu(rtu);
        }
        case kHR_RtuStopBits: {
            if (value != 1 && value != 2) return false;
            ModbusRtuSettings rtu = settingsStore.rtuSnapshot();
            rtu.stopBits = static_cast<uint8_t>(value);
            return settingsStore.setModbusRtu(rtu);
        }

        // ── RTC Clock ────────────────────────────────────────────────────────
        case kHR_RtcYear:   { sRtcYear   = value; return true; }
        case kHR_RtcMonth:  { sRtcMonth  = value; return true; }
        case kHR_RtcDay:    { sRtcDay    = value; return true; }
        case kHR_RtcHour:   { sRtcHour   = value; return true; }
        case kHR_RtcMinute: { sRtcMinute = value; return true; }
        case kHR_RtcSecond: {
            // Writing Second commits accumulated year/month/day/hour/minute
            if (sRtcYear < 2020 || sRtcMonth < 1 || sRtcMonth > 12 ||
                sRtcDay  < 1    || sRtcDay   > 31 || sRtcHour > 23  ||
                sRtcMinute > 59 || value > 59)      return false;
            RtcTime t;
            t.year   = sRtcYear;
            t.month  = static_cast<uint8_t>(sRtcMonth);
            t.date   = static_cast<uint8_t>(sRtcDay);
            t.hour   = static_cast<uint8_t>(sRtcHour);
            t.minute = static_cast<uint8_t>(sRtcMinute);
            t.second = static_cast<uint8_t>(value);
            t.day    = static_cast<uint8_t>(dayOfWeek(t.year, t.month, t.date) + 1);
            rtcService.setTime(t);
            return true;
        }
        case kHR_RtcUnixHi: { sRtcUnixHi = value; return true; }
        case kHR_RtcUnixLo: {
            const uint32_t unix = ((uint32_t)sRtcUnixHi << 16) | value;
            if (unix < 1577836800UL) return false; // reject pre-2020
            const time_t t_val = (time_t)unix;
            struct tm* tm_info = gmtime(&t_val);
            if (!tm_info) return false;
            RtcTime t;
            t.year   = static_cast<uint16_t>(1900 + tm_info->tm_year);
            t.month  = static_cast<uint8_t>(1 + tm_info->tm_mon);
            t.date   = static_cast<uint8_t>(tm_info->tm_mday);
            t.hour   = static_cast<uint8_t>(tm_info->tm_hour);
            t.minute = static_cast<uint8_t>(tm_info->tm_min);
            t.second = static_cast<uint8_t>(tm_info->tm_sec);
            t.day    = static_cast<uint8_t>(tm_info->tm_wday + 1);
            rtcService.setTime(t);
            return true;
        }

        default:
            ResourceMonitor::instance().recordWriteFail("ro_reg");
            return false;
    }
#endif // LPG_MODBUS_WRITES_ENABLED
}

// ── Coil read ─────────────────────────────────────────────────────────────────
uint8_t ModbusRegisterMap::readCoil(uint16_t addr, const StatusSnapshot& status) {
    switch (addr) {
        case kCoil_EstopOk:       return status.emergencyStopOk ? 1 : 0;
        case kCoil_CylinderPres:  return status.cylinderPresent ? 1 : 0;
        case kCoil_NozzleEngaged: return status.nozzleEngaged   ? 1 : 0;
        case kCoil_WeightStable:  return status.weightStable    ? 1 : 0;
        case kCoil_FillActive:    return isFillActive(status.state) ? 1 : 0;
        case kCoil_Relay1:        return status.relays[0] ? 1 : 0;
        case kCoil_Relay2:        return status.relays[1] ? 1 : 0;
        case kCoil_Relay3:        return status.relays[2] ? 1 : 0;
        case kCoil_Relay4:        return status.relays[3] ? 1 : 0;
        case kCoil_Relay5:        return status.relays[4] ? 1 : 0;
        case kCoil_Relay6:        return status.relays[5] ? 1 : 0;
        default:                  return 0;
    }
}

bool ModbusRegisterMap::writeCoil(uint16_t addr, bool value, StatusStore& statusStore) {
    if (addr >= kCoil_Relay1 && addr <= kCoil_Relay6) {
        statusStore.setRelay(static_cast<uint8_t>(addr - kCoil_Relay1), value);
        return true;
    }
    return false;
}

uint8_t ModbusRegisterMap::readDI(uint16_t addr, const StatusSnapshot& status) {
    if (addr < kDI_Count) return status.inputs[addr] ? 1 : 0;
    return 0;
}
