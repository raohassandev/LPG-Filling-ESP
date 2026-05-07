#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_event.h"

static constexpr int kMaxWifiNetworks = 5;
static constexpr const char* kApSsid  = "LPG-Display";
static constexpr const char* kApPass  = "lpg12345";

struct WifiNetwork {
    char ssid[33];
    char pass[65];
};

class WifiManager {
public:
    void begin();
    void poll();   // call periodically from a task

    // Network list
    int             count()           const { return count_; }
    const WifiNetwork& network(int i) const { return networks_[i]; }
    bool addNetwork(const char* ssid, const char* pass);
    bool removeNetwork(int idx);

    // Hotspot
    bool hotspotEnabled() const { return apEnabled_; }
    void setHotspot(bool en);

    // Auto-switch between saved networks when current one drops
    bool autoSwitch() const { return autoSwitch_; }
    void setAutoSwitch(bool en);

    // Status
    bool        connected()    const { return connected_; }
    int         currentIndex() const { return currentIdx_; }
    const char* currentSsid()  const;
    int         rssi()         const { return rssi_; }
    const char* ipAddress()    const { return ip_; }

private:
    WifiNetwork networks_[kMaxWifiNetworks] = {};
    int         count_       = 0;
    bool        apEnabled_   = true;
    bool        autoSwitch_  = true;
    int         currentIdx_  = -1;
    bool        connected_   = false;
    int         rssi_        = 0;
    char        ip_[16]      = "0.0.0.0";

    // Reconnect state machine
    int64_t     connectStartUs_ = 0;
    int         tryIdx_         = 0;
    bool        connecting_     = false;

    static constexpr int64_t kConnectTimeoutUs = 15000000; // 15 s per network

    void loadNvs();
    void saveNvs();
    void startAp();
    void stopAp();
    void startConnect(int idx);
    void onConnected(const char* ip, int rssi);
    void onDisconnected();

    static void wifiEventHandler(void* arg, esp_event_base_t base,
                                 int32_t id, void* data);
    static void ipEventHandler(void* arg, esp_event_base_t base,
                               int32_t id, void* data);

    static WifiManager* s_instance;
};
