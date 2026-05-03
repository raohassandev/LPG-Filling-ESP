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
  settings_.ratePerKg = preferences.getFloat("rate_kg", 250.0f);
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

  if (settings_.ratePerKg <= 0.0f || settings_.ratePerKg > 100000.0f) {
    settings_.ratePerKg = 250.0f;
  }
}

SettingsSnapshot SettingsStore::snapshot() const { return settings_; }

bool SettingsStore::setWifi(const String& staSsid, const String& staPassword) {
  if (staSsid.isEmpty() || staPassword.length() < 8) return false;
  Preferences preferences;
  if (!preferences.begin("lpgctrl", false)) return false;
  preferences.putString("sta_ssid", staSsid);
  preferences.putString("sta_pass", staPassword);
  preferences.end();
  settings_.staSsid    = staSsid;
  settings_.staPassword = staPassword;
  return true;
}

bool SettingsStore::setRatePerKg(float value) {
  if (value <= 0.0f || value > 100000.0f) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin("lpgctrl", false)) {
    return false;
  }

  const bool ok = preferences.putFloat("rate_kg", value) > 0;
  preferences.end();
  if (ok) {
    settings_.ratePerKg = value;
  }
  return ok;
}
