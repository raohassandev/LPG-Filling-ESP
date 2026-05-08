#pragma once

#include <Arduino.h>

#include "NetworkManager.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "TransactionLog.h"

// Define LPG_MQTT_ENABLED=1 in your build flags (sketch #define or platformio.ini)
// to compile MQTT support. Requires the PubSubClient library by Nick O'Leary.
// Without the flag the MqttService compiles to harmless no-ops so the rest of
// the firmware builds with no external libraries.
#ifndef LPG_MQTT_ENABLED
#define LPG_MQTT_ENABLED 0
#endif

#if LPG_MQTT_ENABLED

#include <WiFi.h>
#include <PubSubClient.h>

// Pre-defined public broker presets (stored as preset index in NVS)
namespace MqttBrokerPreset {
  constexpr uint8_t kCustom     = 0;
  constexpr uint8_t kHiveMQ     = 1;  // broker.hivemq.com:1883
  constexpr uint8_t kMosquitto  = 2;  // test.mosquitto.org:1883
  constexpr uint8_t kEmqx       = 3;  // broker.emqx.io:1883
}

class MqttService {
 public:
  MqttService(LpgNetworkManager& networkManager,
              SettingsStore& settingsStore,
              StatusStore& statusStore);

  void begin();
  void loop();

  void publishTransaction(const TransactionRecord& record);
  void publishAlert(const String& alertType, const String& message);
  void publishStatus();
  bool publishTest(String& topicOut, String& messageOut);

  bool isConnected() const { return client_.connected(); }
  String brokerHost() const;
  uint16_t brokerPort() const;

 private:
  bool reconnect();
  void onConnect();
  String buildStatusPayload();
  String buildTransactionPayload(const TransactionRecord& r);
  String topicFor(const String& suffix) const;

  LpgNetworkManager& networkManager_;
  SettingsStore&     settingsStore_;
  StatusStore&       statusStore_;

  WiFiClient   wifiClient_;
  PubSubClient client_;

  unsigned long lastReconnectMs_ = 0;
  unsigned long lastStatusMs_    = 0;
  uint32_t      connectCount_    = 0;

  static constexpr unsigned long kReconnectIntervalMs = 10000;
  static constexpr unsigned long kStatusIntervalMs    = 2000;
};

#else // LPG_MQTT_ENABLED == 0 — stub, no external library needed

class MqttService {
 public:
  MqttService(LpgNetworkManager&, SettingsStore&, StatusStore&) {}
  void begin() { Serial.println(F("[MQTT] Disabled (LPG_MQTT_ENABLED=0)")); }
  void loop() {}
  void publishTransaction(const TransactionRecord&) {}
  void publishAlert(const String&, const String&) {}
  void publishStatus() {}
  bool publishTest(String& topicOut, String& messageOut) {
    topicOut = "";
    messageOut = "MQTT support is disabled at build time";
    return false;
  }
  bool isConnected() const { return false; }
  String brokerHost() const { return ""; }
  uint16_t brokerPort() const { return 0; }
};

#endif // LPG_MQTT_ENABLED
