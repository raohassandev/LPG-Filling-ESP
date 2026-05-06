#include "InputExpander.h"

#include <Wire.h>

InputExpander::InputExpander(const BoardConfig& config) : config_(config) {}

bool InputExpander::begin() {
  poll();
  return true;
}

void InputExpander::poll() {
  Wire.requestFrom(static_cast<int>(config_.kInputExpanderAddress), 1);
  if (Wire.available() < 1) {
    return;
  }

  rawValue_ = Wire.read();
  for (uint8_t i = 0; i < BoardConfig::kInputCount; ++i) {
    states_[i] = (rawValue_ & (1U << i)) == 0;
  }
}

bool InputExpander::inputState(uint8_t index) const {
  if (index >= BoardConfig::kInputCount) {
    return false;
  }
  return states_[index];
}

uint8_t InputExpander::rawValue() const { return rawValue_; }
