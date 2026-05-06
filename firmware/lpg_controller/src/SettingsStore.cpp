#include "SettingsStore.h"

#include <Preferences.h>
#include <esp_efuse.h>
#include <esp_mac.h>

#include "BoardConfig.h"

void SettingsStore::begin() {
  Preferences preferences;
  preferences.begin("lpgctrl", true);
  settings_.staSsid           = preferences.getString("sta_ssid",  BoardConfig::kDefaultStaSsid);
  settings_.staPassword       = preferences.getString("sta_pass",  BoardConfig::kDefaultStaPassword);
  settings_.apSsid            = preferences.getString("ap_ssid",   BoardConfig::kFallbackApSsid);
  settings_.apPassword        = preferences.getString("ap_pass",   BoardConfig::kFallbackApPassword);
  settings_.slowFillThreshold = preferences.getFloat("slow_fill",  0.95f);
  settings_.ratePerKg         = preferences.getFloat("rate_kg",    250.0f);
  settings_.storageMode       = preferences.getUChar("storage",    0);
  preferences.end();

  // MQTT settings stored in a separate NVS namespace to avoid key-count limits
  Preferences mqttPref;
  mqttPref.begin("lpgmqtt", true);
  mqtt_.enabled     = mqttPref.getBool("enabled",  false);
  mqtt_.preset      = mqttPref.getUChar("preset",  1);
  mqtt_.brokerHost  = mqttPref.getString("host",   "");
  mqtt_.brokerPort  = mqttPref.getUShort("port",   1883);
  mqtt_.topicPrefix = mqttPref.getString("prefix", "lpg/controller");
  mqtt_.clientId    = mqttPref.getString("clientid","");
  mqtt_.username    = mqttPref.getString("user",   "");
  mqtt_.password    = mqttPref.getString("pass",   "");
  mqttPref.end();

  // Validate / default WiFi
  if (settings_.staSsid.isEmpty())       settings_.staSsid      = BoardConfig::kDefaultStaSsid;
  if (settings_.staPassword.length() < 8) settings_.staPassword = BoardConfig::kDefaultStaPassword;
  if (settings_.apSsid.isEmpty())          settings_.apSsid = BoardConfig::kFallbackApSsid;

  // Generate a unique per-device AP password from MAC if not yet provisioned
  if (settings_.apPassword.isEmpty() || settings_.apPassword == "lpgsetup123") {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char apPass[9]; apPass[8] = '\0';
    snprintf(apPass, sizeof(apPass), "LP%02X%02X%02X", mac[3], mac[4], mac[5]);
    settings_.apPassword = String(apPass);
    // Persist immediately
    Preferences ap;
    if (ap.begin("lpgctrl", false)) {
      ap.putString("ap_pass", settings_.apPassword);
      ap.end();
    }
    Serial.printf("[SYS] AP password set to device-unique: %s\n", settings_.apPassword.c_str());
  }

  if (settings_.slowFillThreshold < 0.80f || settings_.slowFillThreshold > 0.99f)
    settings_.slowFillThreshold = 0.95f;
  if (settings_.ratePerKg <= 0.0f || settings_.ratePerKg > 100000.0f)
    settings_.ratePerKg = 250.0f;

  // Validate MQTT preset range
  if (mqtt_.preset > 3) mqtt_.preset = 1;

  // RTU settings stored in a separate NVS namespace
  Preferences rtuPref;
  rtuPref.begin("lpgrtu", true);
  rtu_.enabled      = rtuPref.getBool("enabled",  true);
  rtu_.slaveAddress = rtuPref.getUChar("addr",    1);
  rtu_.baudRate     = rtuPref.getUInt("baud",     9600);
  rtu_.parity       = rtuPref.getUChar("parity",  0);
  rtu_.stopBits     = rtuPref.getUChar("stops",   1);
  rtuPref.end();
  if (rtu_.slaveAddress < 1 || rtu_.slaveAddress > 247) rtu_.slaveAddress = 1;
  if (rtu_.parity > 2)                                  rtu_.parity = 0;
  if (rtu_.stopBits != 1 && rtu_.stopBits != 2)         rtu_.stopBits = 1;
}

SettingsSnapshot     SettingsStore::snapshot()     const { return settings_; }
MqttSettingsSnapshot SettingsStore::mqttSnapshot() const { return mqtt_; }
ModbusRtuSettings    SettingsStore::rtuSnapshot()  const { return rtu_; }

bool SettingsStore::setWifi(const String& staSsid, const String& staPassword) {
  if (staSsid.isEmpty() || staPassword.length() < 8) return false;
  Preferences preferences;
  if (!preferences.begin("lpgctrl", false)) return false;
  preferences.putString("sta_ssid", staSsid);
  preferences.putString("sta_pass", staPassword);
  preferences.end();
  settings_.staSsid     = staSsid;
  settings_.staPassword = staPassword;
  return true;
}

bool SettingsStore::setRatePerKg(float value) {
  if (value <= 0.0f || value > 100000.0f) return false;
  Preferences preferences;
  if (!preferences.begin("lpgctrl", false)) return false;
  const bool ok = preferences.putFloat("rate_kg", value) > 0;
  preferences.end();
  if (ok) settings_.ratePerKg = value;
  return ok;
}

bool SettingsStore::setSlowFillThreshold(float value) {
  if (value < 0.80f || value > 0.99f) return false;
  Preferences preferences;
  if (!preferences.begin("lpgctrl", false)) return false;
  const bool ok = preferences.putFloat("slow_fill", value) > 0;
  preferences.end();
  if (ok) settings_.slowFillThreshold = value;
  return ok;
}

bool SettingsStore::setStorageMode(uint8_t mode) {
  if (mode > 2) return false;
  Preferences preferences;
  if (!preferences.begin("lpgctrl", false)) return false;
  preferences.putUChar("storage", mode);
  preferences.end();
  settings_.storageMode = mode;
  return true;
}

bool SettingsStore::setModbusRtu(const ModbusRtuSettings& cfg) {
  if (cfg.slaveAddress < 1 || cfg.slaveAddress > 247) return false;
  if (cfg.parity > 2)                                  return false;
  if (cfg.stopBits != 1 && cfg.stopBits != 2)          return false;
  Preferences rtuPref;
  if (!rtuPref.begin("lpgrtu", false)) return false;
  rtuPref.putBool("enabled",  cfg.enabled);
  rtuPref.putUChar("addr",    cfg.slaveAddress);
  rtuPref.putUInt("baud",     cfg.baudRate);
  rtuPref.putUChar("parity",  cfg.parity);
  rtuPref.putUChar("stops",   cfg.stopBits);
  rtuPref.end();
  rtu_ = cfg;
  return true;
}

bool SettingsStore::setMqtt(const MqttSettingsSnapshot& cfg) {
  Preferences mqttPref;
  if (!mqttPref.begin("lpgmqtt", false)) return false;
  mqttPref.putBool("enabled",   cfg.enabled);
  mqttPref.putUChar("preset",   cfg.preset);
  mqttPref.putString("host",    cfg.brokerHost);
  mqttPref.putUShort("port",    cfg.brokerPort);
  mqttPref.putString("prefix",  cfg.topicPrefix);
  mqttPref.putString("clientid",cfg.clientId);
  mqttPref.putString("user",    cfg.username);
  mqttPref.putString("pass",    cfg.password);
  mqttPref.end();
  mqtt_ = cfg;
  return true;
}
