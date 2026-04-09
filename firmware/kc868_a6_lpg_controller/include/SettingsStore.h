#pragma once

#include <Arduino.h>

struct SettingsSnapshot {
  String apSsid;
  String apPassword;
  float slowFillThreshold = 0.95f;
};

class SettingsStore {
 public:
  void begin();
  SettingsSnapshot snapshot() const;

 private:
  SettingsSnapshot settings_;
};
