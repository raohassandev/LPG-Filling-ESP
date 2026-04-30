#pragma once

#include <Arduino.h>
#include <WiFi.h>

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
    void begin();
    void poll();

    // Mode control
    void setMode(NetworkMode mode);
    NetworkMode currentMode() const { return currentMode_; }

    // STA mode operations
    bool connectSTA(const String &ssid, const String &password);
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

    unsigned long lastStatusChange_ = 0;

    void updateStatus(NetworkStatus newStatus);
    void startAP();
    bool startSTA();
};
