#include "RelayBank.h"

#include <Wire.h>

RelayBank::RelayBank(const BoardConfig& config) : config_(config) {}

bool RelayBank::begin() { return writeAllSafe(); }

bool RelayBank::writeRelay(uint8_t index, bool active) {
  if (index >= BoardConfig::kRelayCount) {
    return false;
  }

  states_[index] = active;
  const uint8_t bit = static_cast<uint8_t>(1U << index);
  const bool physicalLow = config_.kRelaysActiveLow ? active : !active;

  if (physicalLow) {
    shadow_ &= static_cast<uint8_t>(~bit);
  } else {
    shadow_ |= bit;
  }

  return flush();
}

bool RelayBank::writeAllSafe() {
  shadow_ = 0xFF;
  for (bool& state : states_) {
    state = false;
  }
  return flush();
}

bool RelayBank::relayState(uint8_t index) const {
  if (index >= BoardConfig::kRelayCount) {
    return false;
  }
  return states_[index];
}

uint8_t RelayBank::rawShadow() const { return shadow_; }

bool RelayBank::flush() {
  Wire.beginTransmission(config_.kRelayExpanderAddress);
  Wire.write(shadow_);
  return Wire.endTransmission() == 0;
}
