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

  static constexpr const char* kDeviceName = "kc868-a6-lpg";
  static constexpr const char* kDefaultStaSsid     = "";   // Set via first-boot provisioning
  static constexpr const char* kDefaultStaPassword = "";   // Set via first-boot provisioning
  static constexpr const char* kFallbackApSsid     = "LPG-Controller-Setup";
  static constexpr const char* kFallbackApPassword = "";   // Generated per-device in SettingsStore
  static constexpr const char* kFirmwareVersion = "0.1.0";

  // RS-485 / Modbus RTU — UART2 (verify traces on your KC868-A6 PCB revision)
  static constexpr uint8_t kRtuRxPin  = 16;   // UART2 RX
  static constexpr uint8_t kRtuTxPin  = 17;   // UART2 TX
  static constexpr uint8_t kRtuDePin  = 5;    // DE/RE tied together (HIGH=transmit)

  // microSD — SPI2/VSPI (verify KC868-A6 PCB — these pins should be free)
  static constexpr uint8_t kSdMosiPin = 13;
  static constexpr uint8_t kSdMisoPin = 12;
  static constexpr uint8_t kSdClkPin  = 14;
  static constexpr uint8_t kSdCsPin   = 27;
};
