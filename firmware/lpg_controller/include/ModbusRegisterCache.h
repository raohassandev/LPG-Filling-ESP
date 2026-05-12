#pragma once

#include <Arduino.h>
#include "ModbusRegisterMap.h"
#include "ResourceMonitor.h"
#include "RtcService.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "TransactionLog.h"

// RAM-based Modbus register cache.
//
// The FC03/FC04 hot path reads directly from this array — no StatusSnapshot,
// no TransactionLog::computeStats(), no I2C reads happen inside the read path.
//
// Update rates (driven from main loop):
//   updateFast()     — every loop iteration (~1ms):  process/IO/comms/alarm regs
//   updateRtc()      — every ~500 ms:                RTC time registers
//   updateStats()    — every ~5 s:                   transaction statistics
//   updateResource() — every ~1000 ms:               board resource registers
//
// All update functions are non-blocking; the caller controls the intervals.

class ModbusRegisterCache {
 public:
    // Fast process update: sections 0x0000-0x001F and 0x0048-0x0050.
    void updateFast(const StatusSnapshot& s, const SettingsStore& settings, bool mqttConnected);

    // RTC update: section 0x0020-0x0027.
    void updateRtc(const RtcTime& t);

    // Statistics update: sections 0x0028-0x0047.
    // Uses TransactionLog::computeStats() which has its own 30 s TTL cache.
    void updateStats(const TransactionLog& txnLog);

    // Board resource register update: section 0x0051-0x0077.
    void updateResource(const ResourceSnapshot& r);

    // Hot-path read: copy qty registers starting at PDU address startAddr into dest.
    // dest must have qty*2 bytes available. Values are big-endian.
    // Caller is responsible for range-checking before calling.
    void readBlock(uint16_t startAddr, uint16_t qty, uint8_t* dest) const;

    // Single-register read.
    uint16_t readReg(uint16_t addr) const;

    // Coil read (0/1).
    uint8_t readCoil(uint16_t addr) const;

    // Discrete input read (0/1).
    uint8_t readDI(uint16_t addr) const;

 private:
    uint16_t regs_[ModbusRegisterMap::kHR_Count] = {};
    uint8_t  coils_[ModbusRegisterMap::kCoil_Count] = {};
    uint8_t  di_[ModbusRegisterMap::kDI_Count] = {};

    static void setFloat(uint16_t* regs, uint16_t addr, float f);
    static void setUint32(uint16_t* regs, uint16_t addr, uint32_t v);
};
