#pragma once

#include <Arduino.h>

struct SettingsSnapshot {
  String apSsid;
  String apPassword;
  String manufacturingPin;
  float slowFillThreshold = 0.95f;
};

class SettingsStore {
 public:
  void begin();
  SettingsSnapshot snapshot() const;
  bool setManufacturingPin(const String& pin, String& reason);

 private:
  SettingsSnapshot settings_;
};
