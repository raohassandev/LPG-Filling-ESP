#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "SettingsStore.h"

// Network mode
enum class NetworkMode : uint8_t
{
    APOnly = 0,  // AP mode only (commissioning)
    STAOnly = 1, // STA mode only (production)
    Hybrid = 2   // Both AP and STA (fallback)
};

// Connection status
enum class NetworkStatus : uint8_t
{
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
    Failed = 3
};

class LpgNetworkManager
{
public:
    void begin(const SettingsSnapshot& settings);
    void poll();

    // Mode control
    void setMode(NetworkMode mode);
    NetworkMode currentMode() const { return currentMode_; }

    // STA mode operations
    bool connectSTA(const String &ssid, const String &password);
    bool configureWifi(const SettingsSnapshot& settings);
    void disconnectSTA();
    bool isSTAConnected() const;
    String staIP() const;
    String staSSID() const;

    // AP mode operations
    String apIP() const;
    String apSSID() const;

    // Status
    NetworkStatus status() const { return status_; }
    bool isConfigured() const { return staConfigured_; }

    // Auto-reconnect
    void enableAutoReconnect(bool enable);
    bool autoReconnectEnabled() const { return autoReconnect_; }

private:
    NetworkMode currentMode_ = NetworkMode::Hybrid;
    NetworkStatus status_ = NetworkStatus::Disconnected;
    bool staConfigured_ = false;
    bool autoReconnect_ = true;

    String staSsid_;
    String staPassword_;
    String apSsid_;
    String apPassword_;
    bool apEnabled_ = true;
    bool staEnabled_ = true;
    bool autoSwitch_ = true;
    uint8_t wifiCount_ = 0;
    String wifiSsid_[SettingsSnapshot::kMaxWifiNetworks];
    String wifiPassword_[SettingsSnapshot::kMaxWifiNetworks];
    bool wifiEnabled_[SettingsSnapshot::kMaxWifiNetworks] = {true, true, true, true, true};
    bool staDhcp_ = true;
    String staStaticIp_;
    String staGateway_;
    String staSubnet_;
    String staDns1_;
    String staDns2_;

    unsigned long lastStatusChange_      = 0;
    unsigned long lastReconnectAttemptMs_ = 0;

    static constexpr unsigned long kReconnectIntervalMs = 30000; // retry every 30 s

    void updateStatus(NetworkStatus newStatus);
    void startAP();
    bool startSTA();
    bool connectBestSTA();
    bool applyStaIpConfig();
    void startMdns();
};
