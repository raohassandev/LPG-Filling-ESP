#include "NetworkManager.h"
#include "BoardConfig.h"

void LpgNetworkManager::begin()
{
    WiFi.mode(WIFI_AP_STA);
    startAP();
    // STA is started separately via connectSTA()
}

void LpgNetworkManager::poll()
{
    if (currentMode_ == NetworkMode::APOnly) return;

    const wl_status_t wifiStatus = WiFi.status();

    if (wifiStatus == WL_CONNECTED)
    {
        if (status_ != NetworkStatus::Connected)
        {
            updateStatus(NetworkStatus::Connected);
            Serial.printf("[NET] STA connected: %s  IP: %s\n",
                          WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        }
    }
    else if (status_ == NetworkStatus::Connected)
    {
        updateStatus(NetworkStatus::Disconnected);
        Serial.println("[NET] STA disconnected");
        if (autoReconnect_ && staConfigured_)
        {
            updateStatus(NetworkStatus::Connecting);
            WiFi.begin(staSsid_.c_str(), staPassword_.c_str()); // non-blocking
            Serial.println("[NET] STA reconnect initiated");
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

bool LpgNetworkManager::startSTA()
{
    if (!staConfigured_)
    {
        return false;
    }

    updateStatus(NetworkStatus::Connecting);
    WiFi.begin(staSsid_.c_str(), staPassword_.c_str());

    // Wait for connection with timeout
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        return true;
    }

    updateStatus(NetworkStatus::Failed);
    return false;
}

void LpgNetworkManager::updateStatus(NetworkStatus newStatus)
{
    status_ = newStatus;
    lastStatusChange_ = millis();
}
