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
  settings_.staEnabled        = preferences.getBool("sta_en",      true);
  settings_.apEnabled         = preferences.getBool("ap_en",       true);
  settings_.wifiAutoSwitch    = preferences.getBool("wifi_auto",   true);
  settings_.staDhcp           = preferences.getBool("sta_dhcp",    true);
  settings_.staStaticIp       = preferences.getString("sta_ip",     "");
  settings_.staGateway        = preferences.getString("sta_gw",     "");
  settings_.staSubnet         = preferences.getString("sta_sn",     "");
  settings_.staDns1           = preferences.getString("sta_dns1",   "");
  settings_.staDns2           = preferences.getString("sta_dns2",   "");
  settings_.slowFillThreshold = preferences.getFloat("slow_fill",  0.95f);
  settings_.ratePerKg         = preferences.getFloat("rate_kg",    250.0f);
  settings_.storageMode       = preferences.getUChar("storage",    0);
  settings_.stationId         = preferences.getString("station",    "LPG-STN-001");
  settings_.controllerId      = preferences.getString("ctrl_id",    "CTRL-001");
  settings_.siteName          = preferences.getString("site",       "");
  settings_.nozzleId          = preferences.getString("nozzle",     "NOZ-01");
  settings_.commissioningComplete = preferences.getBool("comm_done", false);
  settings_.inputsVerified        = preferences.getBool("in_verify", false);
  settings_.productionLocked      = preferences.getBool("prod_lock", false);
  preferences.end();

  Preferences wifiPref;
  wifiPref.begin("lpgwifi", true);
  settings_.wifiCount = wifiPref.getUChar("count", 0);
  if (settings_.wifiCount > SettingsSnapshot::kMaxWifiNetworks) settings_.wifiCount = 0;
  for (uint8_t i = 0; i < settings_.wifiCount; i++) {
    settings_.wifiSsid[i] = wifiPref.getString(("s" + String(i)).c_str(), "");
    settings_.wifiPassword[i] = wifiPref.getString(("p" + String(i)).c_str(), "");
    settings_.wifiEnabled[i] = wifiPref.getBool(("e" + String(i)).c_str(), true);
  }
  wifiPref.end();

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
  if (settings_.staSsid.isEmpty()) settings_.staSsid = BoardConfig::kDefaultStaSsid;
  if (settings_.staPassword.length() < 8) settings_.staPassword = BoardConfig::kDefaultStaPassword;
  if (settings_.apSsid.isEmpty())          settings_.apSsid = BoardConfig::kFallbackApSsid;

  if (settings_.wifiCount == 0 && !settings_.staSsid.isEmpty() && settings_.staPassword.length() >= 8) {
    settings_.wifiCount = 1;
    settings_.wifiSsid[0] = settings_.staSsid;
    settings_.wifiPassword[0] = settings_.staPassword;
    settings_.wifiEnabled[0] = true;
  }

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

  // RTU settings stored in a separate NVS namespace.
  // Factory default baud is 115200 8N1; existing saved settings are preserved.
  Preferences rtuPref;
  rtuPref.begin("lpgrtu", true);
  rtu_.enabled      = rtuPref.getBool("enabled",  true);
  rtu_.slaveAddress = rtuPref.getUChar("addr",    1);
  rtu_.baudRate     = rtuPref.getUInt("baud",     115200);
  rtu_.parity       = rtuPref.getUChar("parity",  0);
  rtu_.stopBits     = rtuPref.getUChar("stops",   1);
  rtuPref.end();
  if (rtu_.slaveAddress < 1 || rtu_.slaveAddress > 247) rtu_.slaveAddress = 1;
  const uint32_t validBaud[] = {1200,2400,4800,9600,19200,38400,57600,115200};
  bool baudOk = false;
  for (auto v : validBaud) { if (rtu_.baudRate == v) { baudOk = true; break; } }
  if (!baudOk)                                             rtu_.baudRate = 115200;
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
  upsertWifiNetwork(staSsid, staPassword, true);
  return true;
}

bool SettingsStore::setWifiFlags(bool staEnabled, bool apEnabled, bool autoSwitch) {
  Preferences preferences;
  if (!preferences.begin("lpgctrl", false)) return false;
  preferences.putBool("sta_en", staEnabled);
  preferences.putBool("ap_en", apEnabled);
  preferences.putBool("wifi_auto", autoSwitch);
  preferences.end();
  settings_.staEnabled = staEnabled;
  settings_.apEnabled = apEnabled;
  settings_.wifiAutoSwitch = autoSwitch;
  return true;
}

bool SettingsStore::setStaIpConfig(bool dhcp, const String& ip, const String& gateway,
                                   const String& subnet, const String& dns1, const String& dns2) {
  Preferences preferences;
  if (!preferences.begin("lpgctrl", false)) return false;
  preferences.putBool("sta_dhcp", dhcp);
  preferences.putString("sta_ip", ip);
  preferences.putString("sta_gw", gateway);
  preferences.putString("sta_sn", subnet);
  preferences.putString("sta_dns1", dns1);
  preferences.putString("sta_dns2", dns2);
  preferences.end();
  settings_.staDhcp = dhcp;
  settings_.staStaticIp = ip;
  settings_.staGateway = gateway;
  settings_.staSubnet = subnet;
  settings_.staDns1 = dns1;
  settings_.staDns2 = dns2;
  return true;
}

bool SettingsStore::setDeviceIdentity(const String& stationId, const String& controllerId,
                                      const String& siteName, const String& nozzleId) {
  if (stationId.isEmpty() || controllerId.isEmpty() || nozzleId.isEmpty()) return false;
  Preferences preferences;
  if (!preferences.begin("lpgctrl", false)) return false;
  preferences.putString("station", stationId);
  preferences.putString("ctrl_id", controllerId);
  preferences.putString("site", siteName);
  preferences.putString("nozzle", nozzleId);
  preferences.end();
  settings_.stationId = stationId;
  settings_.controllerId = controllerId;
  settings_.siteName = siteName;
  settings_.nozzleId = nozzleId;
  return true;
}

bool SettingsStore::setCommissioningFlags(bool complete, bool inputsVerified, bool productionLocked) {
  Preferences preferences;
  if (!preferences.begin("lpgctrl", false)) return false;
  preferences.putBool("comm_done", complete);
  preferences.putBool("in_verify", inputsVerified);
  preferences.putBool("prod_lock", productionLocked);
  preferences.end();
  settings_.commissioningComplete = complete;
  settings_.inputsVerified = inputsVerified;
  settings_.productionLocked = productionLocked;
  return true;
}

bool SettingsStore::upsertWifiNetwork(const String& ssid, const String& password, bool enabled) {
  if (ssid.isEmpty() || password.length() < 8) return false;
  int8_t idx = -1;
  for (uint8_t i = 0; i < settings_.wifiCount; i++) {
    if (settings_.wifiSsid[i] == ssid) { idx = i; break; }
  }
  if (idx < 0) {
    if (settings_.wifiCount >= SettingsSnapshot::kMaxWifiNetworks) return false;
    idx = settings_.wifiCount++;
  }
  settings_.wifiSsid[idx] = ssid;
  settings_.wifiPassword[idx] = password;
  settings_.wifiEnabled[idx] = enabled;
  settings_.staSsid = settings_.wifiSsid[0];
  settings_.staPassword = settings_.wifiPassword[0];

  Preferences wifiPref;
  if (!wifiPref.begin("lpgwifi", false)) return false;
  wifiPref.putUChar("count", settings_.wifiCount);
  for (uint8_t i = 0; i < settings_.wifiCount; i++) {
    wifiPref.putString(("s" + String(i)).c_str(), settings_.wifiSsid[i]);
    wifiPref.putString(("p" + String(i)).c_str(), settings_.wifiPassword[i]);
    wifiPref.putBool(("e" + String(i)).c_str(), settings_.wifiEnabled[i]);
  }
  wifiPref.end();

  Preferences preferences;
  if (preferences.begin("lpgctrl", false)) {
    preferences.putString("sta_ssid", settings_.staSsid);
    preferences.putString("sta_pass", settings_.staPassword);
    preferences.end();
  }
  return true;
}

bool SettingsStore::removeWifiNetwork(uint8_t index) {
  if (index >= settings_.wifiCount) return false;
  for (uint8_t i = index; i + 1 < settings_.wifiCount; i++) {
    settings_.wifiSsid[i] = settings_.wifiSsid[i + 1];
    settings_.wifiPassword[i] = settings_.wifiPassword[i + 1];
    settings_.wifiEnabled[i] = settings_.wifiEnabled[i + 1];
  }
  settings_.wifiCount--;
  settings_.staSsid = settings_.wifiCount ? settings_.wifiSsid[0] : "";
  settings_.staPassword = settings_.wifiCount ? settings_.wifiPassword[0] : "";

  Preferences wifiPref;
  if (!wifiPref.begin("lpgwifi", false)) return false;
  wifiPref.putUChar("count", settings_.wifiCount);
  for (uint8_t i = 0; i < SettingsSnapshot::kMaxWifiNetworks; i++) {
    if (i < settings_.wifiCount) {
      wifiPref.putString(("s" + String(i)).c_str(), settings_.wifiSsid[i]);
      wifiPref.putString(("p" + String(i)).c_str(), settings_.wifiPassword[i]);
      wifiPref.putBool(("e" + String(i)).c_str(), settings_.wifiEnabled[i]);
    } else {
      wifiPref.remove(("s" + String(i)).c_str());
      wifiPref.remove(("p" + String(i)).c_str());
      wifiPref.remove(("e" + String(i)).c_str());
    }
  }
  wifiPref.end();
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
  const uint32_t validBaud[] = {1200,2400,4800,9600,19200,38400,57600,115200};
  bool baudOk = false;
  for (auto v : validBaud) { if (cfg.baudRate == v) { baudOk = true; break; } }
  if (!baudOk)                                          return false;
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
