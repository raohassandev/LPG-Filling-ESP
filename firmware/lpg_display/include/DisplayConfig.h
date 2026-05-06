#pragma once
#include <Arduino.h>

// Board: Waveshare ESP32-S3-Touch-LCD-5
// Display: 5" IPS 800x480, RGB parallel interface
// Touch:   GT911 (I2C 0x5D)
// RS485:   SP3485 onboard transceiver, auto-direction (no DE/RE pin)

struct DisplayConfig {
  // RS485 — onboard SP3485, routed to GPIO43(TX)/GPIO44(RX)
  static constexpr uint8_t  kRtuRxPin      = 44;
  static constexpr uint8_t  kRtuTxPin      = 43;
  static constexpr uint8_t  kRtuDePin      = 255;   // SP3485 is auto-direction
  static constexpr uint32_t kRtuBaud       = 9600;
  static constexpr uint8_t  kRtuSlaveAddr  = 1;     // KC868-A6 slave address

  // UART for RS485 — use Serial1 (UART1); Serial/UART0 is USB debug
  static constexpr uint8_t  kRtuUartNum    = 1;

  // GT911 touch I2C
  static constexpr uint8_t  kTouchI2cAddr  = 0x5D;

  // Admin PIN (4 digits, shown as dots)
  static constexpr uint32_t kAdminPin      = 1234;

  // Display resolution
  static constexpr uint16_t kDisplayW      = 800;
  static constexpr uint16_t kDisplayH      = 480;
};
