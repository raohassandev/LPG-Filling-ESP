#include "ModbusRegisterMap.h"

namespace {
uint16_t scaledKg(float value) {
  if (value <= 0.0f) {
    return 0;
  }
  if (value > 655.35f) {
    return 65535;
  }
  return static_cast<uint16_t>((value * 100.0f) + 0.5f);
}

uint16_t stateCode(ProcessState state) {
  return static_cast<uint16_t>(state);
}
}  // namespace

uint16_t ModbusRegisterMap::readHoldingRegister(uint16_t address, const StatusSnapshot& status) {
  switch (address) {
    case kLiveWeight:
      return scaledKg(status.liveWeightKg);
    case kTareWeight:
      return scaledKg(status.tareWeightKg);
    case kNetWeight:
      return scaledKg(status.netWeightKg);
    case kFillingStatus:
      return stateCode(status.state);
    case kTargetWeight:
      return scaledKg(status.targetWeightKg);
    case kEstopStatus:
      return status.emergencyStopOk ? 1 : 0;
    default:
      return 0;
  }
}

bool ModbusRegisterMap::writeHoldingRegister(uint16_t address, uint16_t value, StatusStore& statusStore) {
  if (address != kTareWeight) {
    return false;
  }

  statusStore.setTareWeight(static_cast<float>(value) / 100.0f);
  return true;
}
