#include "SettingsStore.h"

#include <Preferences.h>

#include "BoardConfig.h"

void SettingsStore::begin() {
  Preferences preferences;
  preferences.begin("lpgctrl", true);
  settings_.apSsid = preferences.getString("ap_ssid", BoardConfig::kFallbackApSsid);
  settings_.apPassword = preferences.getString("ap_pass", BoardConfig::kFallbackApPassword);
  settings_.slowFillThreshold = preferences.getFloat("slow_fill", 0.95f);
  preferences.end();

  if (settings_.apSsid.isEmpty()) {
    settings_.apSsid = BoardConfig::kFallbackApSsid;
  }

  if (settings_.apPassword.length() < 8) {
    settings_.apPassword = BoardConfig::kFallbackApPassword;
  }

  if (settings_.slowFillThreshold < 0.80f || settings_.slowFillThreshold > 0.99f) {
    settings_.slowFillThreshold = 0.95f;
  }
}

SettingsSnapshot SettingsStore::snapshot() const { return settings_; }
