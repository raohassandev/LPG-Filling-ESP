#pragma once
#include "ModbusClient.h"

// Manages LVGL screens and reacts to ControllerSnapshot changes.
// Screens: Idle → Filling → Complete → Fault
class UiManager {
public:
  void begin();
  void update(const ControllerSnapshot& snap);

private:
  uint8_t lastStateCode_ = 0xFF;

  void showIdle(const ControllerSnapshot& snap);
  void showFilling(const ControllerSnapshot& snap);
  void showComplete(const ControllerSnapshot& snap);
  void showFault(const ControllerSnapshot& snap);
};
