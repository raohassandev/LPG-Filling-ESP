#pragma once
#include <Arduino.h>

// ── ESP32-S3 5" Touch Display board pin map ───────────────────────────────────
// Adjust if your specific board variant differs.
struct DisplayConfig {
  // RS485 (Modbus RTU to KC868-A6)
  static constexpr uint8_t kRtuRxPin  = 18;
  static constexpr uint8_t kRtuTxPin  = 17;
  static constexpr uint8_t kRtuDePin  = 255;  // auto-direction (MAX13487 or similar)
  static constexpr uint32_t kRtuBaud  = 9600;
  static constexpr uint8_t  kRtuAddr  = 1;    // KC868-A6 slave address

  // Display / touch (managed by LVGL BSP — no raw pin access needed here)

  // Status LED
  static constexpr uint8_t kLedPin = 2;
};
