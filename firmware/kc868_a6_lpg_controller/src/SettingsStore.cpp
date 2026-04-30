#include "SettingsStore.h"

#include <Preferences.h>

#include "BoardConfig.h"

void SettingsStore::begin() {
  Preferences preferences;
  preferences.begin("lpgctrl", true);
  settings_.staSsid = preferences.getString("sta_ssid", BoardConfig::kDefaultStaSsid);
  settings_.staPassword = preferences.getString("sta_pass", BoardConfig::kDefaultStaPassword);
  settings_.apSsid = preferences.getString("ap_ssid", BoardConfig::kFallbackApSsid);
  settings_.apPassword = preferences.getString("ap_pass", BoardConfig::kFallbackApPassword);
  settings_.slowFillThreshold = preferences.getFloat("slow_fill", 0.95f);
  preferences.end();

  if (settings_.staSsid.isEmpty()) {
    settings_.staSsid = BoardConfig::kDefaultStaSsid;
  }

  if (settings_.staPassword.length() < 8) {
    settings_.staPassword = BoardConfig::kDefaultStaPassword;
  }

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
