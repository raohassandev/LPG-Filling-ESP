#include "SettingsStore.h"

#include <Preferences.h>

#include "BoardConfig.h"

namespace {
bool isValidManufacturingPin(const String& pin) {
  if (pin.length() < 4 || pin.length() > 8) {
    return false;
  }

  for (size_t i = 0; i < pin.length(); ++i) {
    if (pin[i] < '0' || pin[i] > '9') {
      return false;
    }
  }

  return true;
}
}  // namespace

void SettingsStore::begin() {
  Preferences preferences;
  preferences.begin("lpgctrl", true);
  settings_.apSsid = preferences.getString("ap_ssid", BoardConfig::kFallbackApSsid);
  settings_.apPassword = preferences.getString("ap_pass", BoardConfig::kFallbackApPassword);
  settings_.manufacturingPin = preferences.getString("mfg_pin", BoardConfig::kDefaultManufacturingPin);
  settings_.slowFillThreshold = preferences.getFloat("slow_fill", 0.95f);
  preferences.end();

  if (settings_.apSsid.isEmpty()) {
    settings_.apSsid = BoardConfig::kFallbackApSsid;
  }

  if (settings_.apPassword.length() < 8) {
    settings_.apPassword = BoardConfig::kFallbackApPassword;
  }

  if (!isValidManufacturingPin(settings_.manufacturingPin)) {
    settings_.manufacturingPin = BoardConfig::kDefaultManufacturingPin;
  }

  if (settings_.slowFillThreshold < 0.80f || settings_.slowFillThreshold > 0.99f) {
    settings_.slowFillThreshold = 0.95f;
  }
}

SettingsSnapshot SettingsStore::snapshot() const { return settings_; }
