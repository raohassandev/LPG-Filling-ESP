#include "NetworkManager.h"
#include "BoardConfig.h"
#include <ESPmDNS.h>

void LpgNetworkManager::begin()
{
    WiFi.mode(WIFI_AP_STA);
    startAP();
    // STA is started separately via connectSTA()
}

void LpgNetworkManager::poll()
{
    if (currentMode_ == NetworkMode::APOnly) return;
    if (!staConfigured_) return;

    const wl_status_t wifiStatus = WiFi.status();

    if (wifiStatus == WL_CONNECTED)
    {
        if (status_ != NetworkStatus::Connected)
        {
            updateStatus(NetworkStatus::Connected);
            lastReconnectAttemptMs_ = millis();
            Serial.printf("[NET] STA connected: %s  IP: %s\n",
                          WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
            // Always (re)start mDNS on every fresh connection — first-time or reconnect
            startMdns();
        }
    }
    else if (status_ == NetworkStatus::Connected)
    {
        // Just lost connection
        updateStatus(NetworkStatus::Disconnected);
        Serial.println("[NET] STA disconnected");
        if (autoReconnect_)
        {
            updateStatus(NetworkStatus::Connecting);
            lastReconnectAttemptMs_ = millis();
            WiFi.begin(staSsid_.c_str(), staPassword_.c_str());
            Serial.println("[NET] STA reconnect initiated");
        }
    }
    else if (autoReconnect_ && status_ != NetworkStatus::Connected)
    {
        // Periodic retry when still not connected (handles first-boot timeout fallback
        // and persistent WL_CONNECT_FAILED / WL_NO_SSID_AVAIL states)
        if (millis() - lastReconnectAttemptMs_ >= kReconnectIntervalMs)
        {
            lastReconnectAttemptMs_ = millis();
            updateStatus(NetworkStatus::Connecting);
            WiFi.begin(staSsid_.c_str(), staPassword_.c_str());
            Serial.printf("[NET] STA retry (ssid: %s)\n", staSsid_.c_str());
        }
    }
}

void LpgNetworkManager::setMode(NetworkMode mode)
{
    currentMode_ = mode;

    switch (mode)
    {
    case NetworkMode::APOnly:
        WiFi.mode(WIFI_AP);
        break;
    case NetworkMode::STAOnly:
        WiFi.mode(WIFI_STA);
        break;
    case NetworkMode::Hybrid:
        WiFi.mode(WIFI_AP_STA);
        break;
    }

    Serial.printf("[NET] Mode changed to: %u\n", static_cast<uint8_t>(mode));
}

bool LpgNetworkManager::connectSTA(const String &ssid, const String &password)
{
    if (ssid.isEmpty()) {
        Serial.println("[NET] connectSTA: empty SSID ignored");
        return false;
    }
    staSsid_ = ssid;
    staPassword_ = password;
    staConfigured_ = true;

    return startSTA();
}

void LpgNetworkManager::disconnectSTA()
{
    WiFi.disconnect();
    staConfigured_ = false;
    updateStatus(NetworkStatus::Disconnected);
    Serial.println("[NET] STA disconnected");
}

bool LpgNetworkManager::isSTAConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

String LpgNetworkManager::staIP() const
{
    if (isSTAConnected())
    {
        return WiFi.localIP().toString();
    }
    return String("");
}

String LpgNetworkManager::staSSID() const
{
    if (isSTAConnected())
    {
        return WiFi.SSID();
    }
    return String("");
}

String LpgNetworkManager::apIP() const
{
    return WiFi.softAPIP().toString();
}

String LpgNetworkManager::apSSID() const
{
    return BoardConfig::kFallbackApSsid;
}

void LpgNetworkManager::enableAutoReconnect(bool enable)
{
    autoReconnect_ = enable;
}

void LpgNetworkManager::startAP()
{
    WiFi.softAP(BoardConfig::kFallbackApSsid, BoardConfig::kFallbackApPassword);
    Serial.printf("[NET] AP started: %s\n", BoardConfig::kFallbackApSsid);
    Serial.printf("[NET] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void LpgNetworkManager::startMdns()
{
    MDNS.end();
    delay(100);
    if (MDNS.begin("lpg-controller")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("[NET] mDNS started: lpg-controller.local");
    } else {
        Serial.println("[NET] mDNS start failed");
    }
}

bool LpgNetworkManager::startSTA()
{
    if (!staConfigured_)
    {
        return false;
    }

    updateStatus(NetworkStatus::Connecting);
    lastReconnectAttemptMs_ = millis();
    WiFi.begin(staSsid_.c_str(), staPassword_.c_str());
    Serial.printf("[NET] STA connecting to: %s\n", staSsid_.c_str());
    return true; // poll() will detect WL_CONNECTED and start mDNS
}

void LpgNetworkManager::updateStatus(NetworkStatus newStatus)
{
    status_ = newStatus;
    lastStatusChange_ = millis();
}
