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

 private:
  SettingsSnapshot settings_;
};
