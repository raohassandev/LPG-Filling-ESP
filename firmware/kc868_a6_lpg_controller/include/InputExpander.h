#pragma once

#include <Arduino.h>

#include "BoardConfig.h"

class InputExpander {
 public:
  explicit InputExpander(const BoardConfig& config);

  bool begin();
  void poll();
  bool inputState(uint8_t index) const;
  uint8_t rawValue() const;

 private:
  const BoardConfig& config_;
  uint8_t rawValue_ = 0xFF;
  bool states_[BoardConfig::kInputCount] = {false, false, false, false, false, false};
};
