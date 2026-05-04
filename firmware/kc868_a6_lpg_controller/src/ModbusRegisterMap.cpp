#include "ModbusRegisterMap.h"

namespace {

uint16_t scaledKg(float value) {
    if (value <= 0.0f) return 0;
    if (value > 655.35f) return 65535;
    return static_cast<uint16_t>((value * 100.0f) + 0.5f);
}

uint16_t scaledPkr100(float value) {
    // Rate per kg stored as PKR × 100 (e.g. 250.00 → 25000)
    if (value <= 0.0f) return 0;
    if (value > 655.35f) return 65535;
    return static_cast<uint16_t>((value * 100.0f) + 0.5f);
}

uint16_t pkrInt(float value) {
    // Amount stored as integer PKR (e.g. 2950.25 → 2950)
    if (value <= 0.0f) return 0;
    if (value > 65535.0f) return 65535;
    return static_cast<uint16_t>(value);
}

}  // namespace

uint16_t ModbusRegisterMap::readHoldingRegister(uint16_t address,
                                                 const StatusSnapshot& status,
                                                 const TransactionLog& txnLog) {
    switch (address) {
        case kLiveWeight:       return scaledKg(status.liveWeightKg);
        case kTareWeight:       return scaledKg(status.tareWeightKg);
        case kNetWeight:        return scaledKg(status.netWeightKg);
        case kFillingStatus:    return static_cast<uint16_t>(status.state);
        case kTargetWeight:     return scaledKg(status.targetWeightKg);
        case kEstopStatus:      return status.emergencyStopOk ? 1 : 0;
        case kRatePerKg:        return scaledPkr100(status.ratePerKg);
        case kTargetAmount:     return pkrInt(status.targetAmount);
        case kCurrentAmount:    return pkrInt(status.netWeightKg * status.ratePerKg);
        case kTransactionCount: return static_cast<uint16_t>(txnLog.totalCount() & 0xFFFF);
        case kCylinderPresent:  return status.cylinderPresent ? 1 : 0;
        case kNozzleEngaged:    return status.nozzleEngaged ? 1 : 0;
        case kWeightStable:     return status.weightStable ? 1 : 0;
        case kUptimeSec:        return static_cast<uint16_t>((millis() / 1000) & 0xFFFF);
        case kCommand:          return 0;  // write-only, always reads 0
        default:                return 0;
    }
}

bool ModbusRegisterMap::writeHoldingRegister(uint16_t address, uint16_t value,
                                              StatusStore& statusStore,
                                              SettingsStore& settingsStore,
                                              FillController& fillController) {
    const StatusSnapshot snap = statusStore.snapshot();

    switch (address) {

        case kTareWeight:
            statusStore.setTareWeight(static_cast<float>(value) / 100.0f);
            return true;

        case kTargetWeight: {
            const float kg = static_cast<float>(value) / 100.0f;
            statusStore.setTargets(kg, snap.targetAmount, snap.ratePerKg);
            return true;
        }

        case kRatePerKg: {
            const float rate = static_cast<float>(value) / 100.0f;
            if (rate <= 0.0f) return false;
            settingsStore.setRatePerKg(rate);
            statusStore.setTargets(snap.targetWeightKg, snap.targetAmount, rate);
            return true;
        }

        case kTargetAmount: {
            const float amount = static_cast<float>(value);
            statusStore.setTargets(snap.targetWeightKg, amount, snap.ratePerKg);
            return true;
        }

        case kCommand: {
            String reason;
            switch (value) {
                case 1:  // Start fill — uses Target Weight, Rate, Target Amount already set
                    fillController.startFill(snap.targetWeightKg, snap.ratePerKg,
                                             snap.targetAmount, reason, "modbus");
                    return true;
                case 2:  // Stop fill
                    fillController.stopFill("modbus_stop");
                    return true;
                case 3:  // Reset to idle
                    fillController.resetToIdle(reason);
                    return true;
                case 4:  // Zero net weight (tare from live)
                    statusStore.setTareWeight(snap.liveWeightKg);
                    return true;
                default:
                    return false;
            }
        }

        default:
            return false;  // read-only or unknown
    }
}
