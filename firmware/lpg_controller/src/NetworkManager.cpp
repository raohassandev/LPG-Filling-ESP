#include "NetworkManager.h"
#include "BoardConfig.h"
#include <ESPmDNS.h>

void LpgNetworkManager::begin(const SettingsSnapshot& settings)
{
    configureWifi(settings);
    if (apEnabled_ && staEnabled_) WiFi.mode(WIFI_AP_STA);
    else if (apEnabled_)          WiFi.mode(WIFI_AP);
    else                          WiFi.mode(WIFI_STA);
    if (apEnabled_) startAP();
}

void LpgNetworkManager::poll()
{
    if (currentMode_ == NetworkMode::APOnly) return;
    if (!staEnabled_) return;
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
            if (autoSwitch_) connectBestSTA();
            else WiFi.begin(staSsid_.c_str(), staPassword_.c_str());
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
            if (autoSwitch_) connectBestSTA();
            else WiFi.begin(staSsid_.c_str(), staPassword_.c_str());
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
    if (!staEnabled_) {
        Serial.println("[NET] connectSTA: STA disabled");
        return false;
    }
    if (ssid.isEmpty()) {
        Serial.println("[NET] connectSTA: empty SSID ignored");
        return false;
    }
    staSsid_ = ssid;
    staPassword_ = password;
    staConfigured_ = true;

    return startSTA();
}

bool LpgNetworkManager::configureWifi(const SettingsSnapshot& settings)
{
    staEnabled_ = settings.staEnabled;
    apEnabled_ = settings.apEnabled;
    autoSwitch_ = settings.wifiAutoSwitch;
    apSsid_ = settings.apSsid;
    apPassword_ = settings.apPassword;
    wifiCount_ = settings.wifiCount;
    if (wifiCount_ > SettingsSnapshot::kMaxWifiNetworks) wifiCount_ = SettingsSnapshot::kMaxWifiNetworks;
    for (uint8_t i = 0; i < wifiCount_; i++) {
        wifiSsid_[i] = settings.wifiSsid[i];
        wifiPassword_[i] = settings.wifiPassword[i];
        wifiEnabled_[i] = settings.wifiEnabled[i];
    }
    if (wifiCount_ > 0) {
        staSsid_ = wifiSsid_[0];
        staPassword_ = wifiPassword_[0];
        staConfigured_ = true;
    }
    return true;
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
    return apSsid_;
}

void LpgNetworkManager::enableAutoReconnect(bool enable)
{
    autoReconnect_ = enable;
}

void LpgNetworkManager::startAP()
{
    if (apPassword_.length() >= 8) WiFi.softAP(apSsid_.c_str(), apPassword_.c_str());
    else                           WiFi.softAP(apSsid_.c_str());
    Serial.printf("[NET] AP started: %s\n", apSsid_.c_str());
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
    if (!staEnabled_ || !staConfigured_)
    {
        return false;
    }

    updateStatus(NetworkStatus::Connecting);
    lastReconnectAttemptMs_ = millis();
    if (autoSwitch_ && connectBestSTA()) return true;
    WiFi.begin(staSsid_.c_str(), staPassword_.c_str());
    Serial.printf("[NET] STA connecting to: %s\n", staSsid_.c_str());
    return true; // poll() will detect WL_CONNECTED and start mDNS
}

bool LpgNetworkManager::connectBestSTA()
{
    if (!staEnabled_ || wifiCount_ == 0) return false;
    int selected = -1;
    const int found = WiFi.scanNetworks(false, true);
    if (found > 0) {
        for (uint8_t saved = 0; saved < wifiCount_ && selected < 0; saved++) {
            if (!wifiEnabled_[saved] || wifiSsid_[saved].isEmpty()) continue;
            for (int i = 0; i < found; i++) {
                if (WiFi.SSID(i) == wifiSsid_[saved]) {
                    selected = saved;
                    break;
                }
            }
        }
    }
    WiFi.scanDelete();
    if (selected < 0) {
        for (uint8_t i = 0; i < wifiCount_; i++) {
            if (wifiEnabled_[i] && !wifiSsid_[i].isEmpty()) { selected = i; break; }
        }
    }
    if (selected < 0) return false;
    staSsid_ = wifiSsid_[selected];
    staPassword_ = wifiPassword_[selected];
    WiFi.begin(staSsid_.c_str(), staPassword_.c_str());
    Serial.printf("[NET] STA auto-select: %s\n", staSsid_.c_str());
    return true;
}

void LpgNetworkManager::updateStatus(NetworkStatus newStatus)
{
    status_ = newStatus;
    lastStatusChange_ = millis();
}
