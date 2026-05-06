#pragma once

#include <Arduino.h>

struct SettingsSnapshot {
  String staSsid;
  String staPassword;
  String apSsid;
  String apPassword;
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
  bool setMqtt(const MqttSettingsSnapshot& cfg);
  bool setModbusRtu(const ModbusRtuSettings& cfg);

 private:
  SettingsSnapshot     settings_;
  MqttSettingsSnapshot mqtt_;
  ModbusRtuSettings    rtu_;
};
