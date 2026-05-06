#pragma once
#include <Arduino.h>

struct DisplayConfig {
  // RS485 — Modbus RTU master to KC868-A6
  static constexpr uint8_t  kRtuRxPin      = 18;
  static constexpr uint8_t  kRtuTxPin      = 17;
  static constexpr uint8_t  kRtuDePin      = 255;   // 255 = auto-direction (no DE/RE pin)
  static constexpr uint32_t kRtuBaud       = 9600;
  static constexpr uint8_t  kRtuSlaveAddr  = 1;     // KC868-A6 slave address

  // Admin PIN (4 digits, shown as dots)
  static constexpr uint32_t kAdminPin      = 1234;

  // Display resolution
  static constexpr uint16_t kDisplayW      = 800;
  static constexpr uint16_t kDisplayH      = 480;

  // Status LED
  static constexpr uint8_t  kLedPin        = 2;
};
