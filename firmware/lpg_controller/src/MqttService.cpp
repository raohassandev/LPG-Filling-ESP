#include "MqttService.h"

#if LPG_MQTT_ENABLED

// ── Public broker host/port tables ───────────────────────────────────────────
static const char* kBrokerHosts[] = {
    "",                       // 0 = Custom (taken from settings)
    "broker.hivemq.com",      // 1 = HiveMQ public
    "test.mosquitto.org",     // 2 = Eclipse Mosquitto test
    "broker.emqx.io",         // 3 = EMQX public
};
static const uint16_t kBrokerPorts[] = { 0, 1883, 1883, 1883 };

MqttService::MqttService(LpgNetworkManager& networkManager,
                         SettingsStore& settingsStore,
                         StatusStore& statusStore)
    : networkManager_(networkManager),
      settingsStore_(settingsStore),
      statusStore_(statusStore),
      client_(wifiClient_) {}

void MqttService::begin() {
    Serial.println(F("[MQTT] Service initialised"));
    client_.setKeepAlive(30);
    client_.setSocketTimeout(5);
    // Buffer large enough for a full status payload
    client_.setBufferSize(768);
}

// Called every loop iteration
void MqttService::loop() {
    const MqttSettingsSnapshot cfg = settingsStore_.mqttSnapshot();
    if (!cfg.enabled) return;
    if (!networkManager_.isSTAConnected()) return;

    if (!client_.connected()) {
        const unsigned long now = millis();
        if (now - lastReconnectMs_ < kReconnectIntervalMs) return;
        lastReconnectMs_ = now;
        reconnect();
    }

    client_.loop();

    // Periodic status publish
    if (client_.connected()) {
        const unsigned long now = millis();
        if (now - lastStatusMs_ >= kStatusIntervalMs) {
            lastStatusMs_ = now;
            publishStatus();
        }
    }
}

bool MqttService::reconnect() {
    const MqttSettingsSnapshot cfg = settingsStore_.mqttSnapshot();
    const String host = brokerHost();
    const uint16_t port = brokerPort();
    if (host.isEmpty()) return false;

    client_.setServer(host.c_str(), port);

    const String clientId = cfg.clientId.isEmpty()
        ? "lpg-" + String((uint32_t)ESP.getEfuseMac(), HEX)
        : cfg.clientId;

    const String lwtTopic = topicFor("lwt");
    const bool ok = cfg.username.isEmpty()
        ? client_.connect(clientId.c_str(), lwtTopic.c_str(), 0, true, "offline")
        : client_.connect(clientId.c_str(),
                          cfg.username.c_str(), cfg.password.c_str(),
                          lwtTopic.c_str(), 0, true, "offline");

    if (ok) {
        connectCount_++;
        onConnect();
        Serial.printf("[MQTT] Connected to %s:%u (client=%s)\n",
                      host.c_str(), port, clientId.c_str());
    } else {
        Serial.printf("[MQTT] Connect failed, rc=%d (retry in %lus)\n",
                      client_.state(), kReconnectIntervalMs / 1000);
    }
    return ok;
}

void MqttService::onConnect() {
    // Publish online will-replace
    client_.publish(topicFor("lwt").c_str(), "online", true);
}

void MqttService::publishStatus() {
    if (!client_.connected()) return;
    const String payload = buildStatusPayload();
    client_.publish(topicFor("status").c_str(), payload.c_str(), false);
}

bool MqttService::publishTest(String& topicOut, String& messageOut) {
    const MqttSettingsSnapshot cfg = settingsStore_.mqttSnapshot();
    if (!cfg.enabled) {
        messageOut = "MQTT is disabled";
        return false;
    }
    if (!networkManager_.isSTAConnected()) {
        messageOut = "WiFi STA is not connected";
        return false;
    }
    if (!client_.connected() && !reconnect()) {
        messageOut = "MQTT broker connection failed";
        return false;
    }

    topicOut = topicFor("status");
    const String payload = buildStatusPayload();
    const bool ok = client_.publish(topicOut.c_str(), payload.c_str(), false);
    messageOut = ok ? "test publish sent" : "publish failed";
    return ok;
}

void MqttService::publishTransaction(const TransactionRecord& record) {
    if (!client_.connected()) return;
    const String payload = buildTransactionPayload(record);
    client_.publish(topicFor("transaction").c_str(), payload.c_str(), false);
    Serial.printf("[MQTT] Transaction %u published\n", record.id);
}

void MqttService::publishAlert(const String& alertType, const String& message) {
    if (!client_.connected()) return;
    String payload = "{\"type\":\"" + alertType + "\",\"message\":\"" + message + "\"}";
    client_.publish(topicFor("alert").c_str(), payload.c_str(), false);
    Serial.printf("[MQTT] Alert published: %s\n", alertType.c_str());
}

String MqttService::buildStatusPayload() {
    const StatusSnapshot s = statusStore_.snapshot();
    const SettingsSnapshot cfg = settingsStore_.snapshot();
    char buf[640];
    snprintf(buf, sizeof(buf),
        "{\"deviceType\":\"lpg-controller\",\"stationId\":\"%s\",\"controllerId\":\"%s\","
        "\"siteName\":\"%s\",\"nozzleId\":\"%s\",\"ts\":%lu,"
        "\"state\":\"%s\",\"liveKg\":%.3f,\"netKg\":%.3f,\"targetKg\":%.3f,"
        "\"rate\":%.2f,\"amount\":%.2f,"
        "\"estopOk\":%s,\"cylinderPresent\":%s,\"nozzleEngaged\":%s,\"weightStable\":%s,"
        "\"alarmCode\":%u,\"alarmSeverity\":%u,\"readinessMask\":%u,\"blockerMask\":%u,"
        "\"uptimeSec\":%lu}",
        cfg.stationId.c_str(), cfg.controllerId.c_str(), cfg.siteName.c_str(), cfg.nozzleId.c_str(),
        millis() / 1000UL,
        s.stateLabel.c_str(),
        s.liveWeightKg, s.netWeightKg, s.targetWeightKg, s.ratePerKg, s.netWeightKg * s.ratePerKg,
        s.emergencyStopOk  ? "true" : "false",
        s.cylinderPresent  ? "true" : "false",
        s.nozzleEngaged    ? "true" : "false",
        s.weightStable     ? "true" : "false", s.alarmCode, s.alarmSeverity,
        s.readinessMask, s.blockerMask,
        millis() / 1000UL);
    return String(buf);
}

String MqttService::buildTransactionPayload(const TransactionRecord& r) {
    char buf[384];
    snprintf(buf, sizeof(buf),
        "{\"id\":%u,\"txnId\":\"%s\",\"start\":%u,\"end\":%u,"
        "\"targetKg\":%.3f,\"finalKg\":%.3f,\"netKg\":%.3f,"
        "\"rate\":%.2f,\"amount\":%.2f,\"status\":%u,\"operator\":\"%s\"}",
        r.id, r.transactionId.c_str(),
        r.startTimeUnix, r.endTimeUnix,
        r.targetWeightKg, r.finalWeightKg, r.netWeightKg,
        r.ratePerKg, r.finalAmount,
        static_cast<uint8_t>(r.status),
        r.operatorUsername.c_str());
    return String(buf);
}

String MqttService::topicFor(const String& suffix) const {
    const MqttSettingsSnapshot cfg = settingsStore_.mqttSnapshot();
    const SettingsSnapshot settings = settingsStore_.snapshot();
    const String prefix = cfg.topicPrefix.isEmpty() ? "lpg" : cfg.topicPrefix;
    return prefix + "/" + settings.stationId + "/" + settings.controllerId + "/" + suffix;
}

String MqttService::brokerHost() const {
    const MqttSettingsSnapshot cfg = settingsStore_.mqttSnapshot();
    if (cfg.preset == MqttBrokerPreset::kCustom) return cfg.brokerHost;
    if (cfg.preset < 4) return String(kBrokerHosts[cfg.preset]);
    return "";
}

uint16_t MqttService::brokerPort() const {
    const MqttSettingsSnapshot cfg = settingsStore_.mqttSnapshot();
    if (cfg.preset == MqttBrokerPreset::kCustom) return cfg.brokerPort > 0 ? cfg.brokerPort : 1883;
    if (cfg.preset < 4) return kBrokerPorts[cfg.preset];
    return 1883;
}

#endif // LPG_MQTT_ENABLED
