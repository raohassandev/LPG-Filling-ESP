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
  static constexpr const char* kFallbackApSsid = "LPG-Controller-Setup";
  static constexpr const char* kFallbackApPassword = "lpgsetup123";
  static constexpr const char* kDefaultManufacturingPin = "2468";
  static constexpr const char* kFirmwareVersion = "0.2.0-ota";
};
