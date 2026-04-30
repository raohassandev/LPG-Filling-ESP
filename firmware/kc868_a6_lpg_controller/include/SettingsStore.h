#pragma once

#include <Arduino.h>

struct SettingsSnapshot {
  String staSsid;
  String staPassword;
  String apSsid;
  String apPassword;
  float slowFillThreshold = 0.95f;
  float ratePerKg = 250.0f;
};

class SettingsStore {
 public:
  void begin();
  SettingsSnapshot snapshot() const;
  bool setRatePerKg(float value);

 private:
  SettingsSnapshot settings_;
};
