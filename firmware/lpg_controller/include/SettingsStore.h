#pragma once

#include <Arduino.h>

struct SettingsSnapshot {
  static constexpr uint8_t kMaxWifiNetworks = 5;

  String staSsid;
  String staPassword;
  String apSsid;
  String apPassword;
  bool   staEnabled        = true;
  bool   apEnabled         = true;
  bool   wifiAutoSwitch    = true;
  uint8_t wifiCount        = 0;
  String wifiSsid[kMaxWifiNetworks];
  String wifiPassword[kMaxWifiNetworks];
  bool   wifiEnabled[kMaxWifiNetworks] = {true, true, true, true, true};
  float   slowFillThreshold = 0.95f;
  float   ratePerKg         = 250.0f;
  uint8_t storageMode       = 0;  // 0=SPIFFS only  1=SD only  2=Both
};

struct MqttSettingsSnapshot {
  bool     enabled     = false;
  uint8_t  preset      = 1;        // 0=Custom 1=HiveMQ 2=Mosquitto 3=EMQX
  String   brokerHost;             // used when preset=0
  uint16_t brokerPort  = 1883;     // used when preset=0
  String   topicPrefix = "lpg/controller";
  String   clientId;               // auto-generated if empty
  String   username;
  String   password;
};

struct ModbusRtuSettings {
  bool     enabled      = true;
  uint8_t  slaveAddress = 1;     // 1–247
  uint32_t baudRate     = 9600;  // 9600/19200/38400/57600/115200
  uint8_t  parity       = 0;    // 0=None 1=Even 2=Odd
  uint8_t  stopBits     = 1;    // 1 or 2
};

class SettingsStore {
 public:
  void begin();
  SettingsSnapshot     snapshot()     const;
  MqttSettingsSnapshot mqttSnapshot() const;
  ModbusRtuSettings    rtuSnapshot()  const;

  bool setRatePerKg(float value);
  bool setSlowFillThreshold(float value);
  bool setStorageMode(uint8_t mode);
  bool setWifi(const String& staSsid, const String& staPassword);
  bool setWifiFlags(bool staEnabled, bool apEnabled, bool autoSwitch);
  bool upsertWifiNetwork(const String& ssid, const String& password, bool enabled);
  bool removeWifiNetwork(uint8_t index);
  bool setMqtt(const MqttSettingsSnapshot& cfg);
  bool setModbusRtu(const ModbusRtuSettings& cfg);

 private:
  SettingsSnapshot     settings_;
  MqttSettingsSnapshot mqtt_;
  ModbusRtuSettings    rtu_;
};
