#pragma once

#include <Arduino.h>

#include "BoardConfig.h"

class RelayBank {
 public:
  explicit RelayBank(const BoardConfig& config);

  bool begin();
  bool writeRelay(uint8_t index, bool active);
  bool writeAllSafe();
  bool relayState(uint8_t index) const;
  uint8_t rawShadow() const;

 private:
  bool flush();

  const BoardConfig& config_;
  bool states_[BoardConfig::kRelayCount] = {false, false, false, false, false, false};
  uint8_t shadow_ = 0xFF;
};
