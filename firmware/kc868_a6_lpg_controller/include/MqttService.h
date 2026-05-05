#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "NetworkManager.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "TransactionLog.h"

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

  // Publish events — called from main loop on state transitions
  void publishTransaction(const TransactionRecord& record);
  void publishAlert(const String& alertType, const String& message);
  void publishStatus();  // called every ~2 s from loop

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
