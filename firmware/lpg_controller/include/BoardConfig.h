#pragma once

#include <Arduino.h>

struct BoardConfig {
  static constexpr uint8_t kI2cSdaPin = 4;
  static constexpr uint8_t kI2cSclPin = 15;

  static constexpr uint8_t kRelayExpanderAddress = 0x24;
  static constexpr uint8_t kInputExpanderAddress = 0x22;
  static constexpr uint8_t kOledAddress = 0x3C;
  static constexpr uint8_t kRtcAddress = 0x68;

  static constexpr bool kRelaysActiveLow = true;
  static constexpr uint8_t kRelayCount = 6;
  static constexpr uint8_t kInputCount = 6;

  // Runtime input mapping verified on the current prototype wiring.
  // inputState(index) returns true when the PCF8574 input is active.
  static constexpr uint8_t kInputCylinderPresent = 0;
  static constexpr uint8_t kInputNozzleEngaged   = 1;
  static constexpr uint8_t kInputEmergencyStop   = 3;
  static constexpr bool kInputEmergencyRawMeansTripped = true;

  static constexpr const char* kDeviceName = "kc868-a6-lpg";
  static constexpr const char* kDefaultStaSsid     = "Rao";
  static constexpr const char* kDefaultStaPassword = "password123";
  static constexpr const char* kFallbackApSsid     = "LPG-Controller-Setup";
  static constexpr const char* kFallbackApPassword = "";   // Generated per-device in SettingsStore
  static constexpr const char* kFirmwareVersion = "0.1.0";

  // RS-485 / Modbus RTU — UART2
  // KC868-A6 RS485 port: GPIO27=TX, GPIO14=RX (NOT the RS232 port on GPIO16/17)
  // The board uses an auto-direction RS485 transceiver — no DE/RE pin needed.
  static constexpr uint8_t kRtuRxPin  = 14;    // UART2 RX → RS485 RO
  static constexpr uint8_t kRtuTxPin  = 27;    // UART2 TX → RS485 DI
  static constexpr uint8_t kRtuDePin  = 255;   // 255 = not connected (auto-direction transceiver)

  // microSD — VSPI (GPIO18/23/19/5 per KC868-A6 schematic)
  static constexpr uint8_t kSdMosiPin = 23;
  static constexpr uint8_t kSdMisoPin = 19;
  static constexpr uint8_t kSdClkPin  = 18;
  static constexpr uint8_t kSdCsPin   = 5;
};
