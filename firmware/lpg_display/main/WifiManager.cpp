#include "WifiManager.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "WIFI";
WifiManager* WifiManager::s_instance = nullptr;

// ── NVS ──────────────────────────────────────────────────────────────────────

void WifiManager::loadNvs() {
    nvs_handle_t h;
    if (nvs_open("wifi_cfg", NVS_READONLY, &h) != ESP_OK) return;

    uint8_t cnt = 0;
    nvs_get_u8(h, "count", &cnt);
    count_ = (cnt > kMaxWifiNetworks) ? kMaxWifiNetworks : cnt;

    for (int i = 0; i < count_; i++) {
        char key[16];
        size_t sz = sizeof(networks_[i].ssid);
        snprintf(key, sizeof(key), "ssid%d", i);
        nvs_get_str(h, key, networks_[i].ssid, &sz);
        sz = sizeof(networks_[i].pass);
        snprintf(key, sizeof(key), "pass%d", i);
        nvs_get_str(h, key, networks_[i].pass, &sz);
    }

    uint8_t ap = 1, sw = 1;
    nvs_get_u8(h, "ap_en",    &ap);
    nvs_get_u8(h, "auto_sw",  &sw);
    apEnabled_  = ap;
    autoSwitch_ = sw;
    nvs_close(h);
    ESP_LOGI(TAG, "Loaded %d networks from NVS", count_);
}

void WifiManager::saveNvs() {
    nvs_handle_t h;
    if (nvs_open("wifi_cfg", NVS_READWRITE, &h) != ESP_OK) return;

    nvs_set_u8(h, "count", (uint8_t)count_);
    for (int i = 0; i < count_; i++) {
        char key[16];
        snprintf(key, sizeof(key), "ssid%d", i);
        nvs_set_str(h, key, networks_[i].ssid);
        snprintf(key, sizeof(key), "pass%d", i);
        nvs_set_str(h, key, networks_[i].pass);
    }
    nvs_set_u8(h, "ap_en",   apEnabled_  ? 1 : 0);
    nvs_set_u8(h, "auto_sw", autoSwitch_ ? 1 : 0);
    nvs_commit(h);
    nvs_close(h);
}

// ── Event handlers ────────────────────────────────────────────────────────────

void WifiManager::wifiEventHandler(void* arg, esp_event_base_t base,
                                   int32_t id, void* data) {
    if (!s_instance) return;
    WifiManager& w = *s_instance;

    if (id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "STA disconnected");
        w.onDisconnected();
    } else if (id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "Client connected to AP");
    }
}

void WifiManager::ipEventHandler(void* arg, esp_event_base_t base,
                                 int32_t id, void* data) {
    if (!s_instance) return;
    WifiManager& w = *s_instance;

    if (id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* ev = (ip_event_got_ip_t*)data;
        char ip[16];
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ev->ip_info.ip));

        wifi_ap_record_t ap{};
        int r = 0;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) r = ap.rssi;

        w.onConnected(ip, r);
    }
}

// ── Init ──────────────────────────────────────────────────────────────────────

void WifiManager::begin() {
    s_instance = this;

    // NVS must be initialised before WiFi (controller code may have done it)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    loadNvs();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        wifiEventHandler, nullptr, nullptr);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        ipEventHandler, nullptr, nullptr);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    if (apEnabled_) startAp();

    if (count_ > 0) startConnect(0);
    else            ESP_ERROR_CHECK(esp_wifi_start());
}

// ── AP ────────────────────────────────────────────────────────────────────────

void WifiManager::startAp() {
    wifi_config_t ap_cfg = {};
    strncpy((char*)ap_cfg.ap.ssid,     kApSsid, sizeof(ap_cfg.ap.ssid));
    strncpy((char*)ap_cfg.ap.password, kApPass, sizeof(ap_cfg.ap.password));
    ap_cfg.ap.ssid_len       = (uint8_t)strlen(kApSsid);
    ap_cfg.ap.channel        = 6;
    ap_cfg.ap.authmode       = WIFI_AUTH_WPA2_PSK;
    ap_cfg.ap.max_connection = 4;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_LOGI(TAG, "AP started: %s", kApSsid);
}

void WifiManager::stopAp() {
    wifi_config_t ap_cfg = {};
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
}

void WifiManager::setHotspot(bool en) {
    apEnabled_ = en;
    saveNvs();
    if (en) startAp();
    else    stopAp();
    esp_wifi_start(); // re-apply
}

// ── STA ───────────────────────────────────────────────────────────────────────

void WifiManager::startConnect(int idx) {
    if (idx < 0 || idx >= count_) return;
    connecting_     = true;
    currentIdx_     = idx;
    connectStartUs_ = esp_timer_get_time();

    wifi_config_t cfg = {};
    strncpy((char*)cfg.sta.ssid,     networks_[idx].ssid, sizeof(cfg.sta.ssid));
    strncpy((char*)cfg.sta.password, networks_[idx].pass, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_start();
    esp_wifi_connect();
    ESP_LOGI(TAG, "Connecting to '%s' (%d/%d)", networks_[idx].ssid, idx+1, count_);
}

void WifiManager::onConnected(const char* ip, int rssi) {
    connected_  = true;
    connecting_ = false;
    rssi_       = rssi;
    strncpy(ip_, ip, sizeof(ip_));
    ESP_LOGI(TAG, "Connected: %s  RSSI=%d  IP=%s",
             networks_[currentIdx_].ssid, rssi_, ip_);
}

void WifiManager::onDisconnected() {
    connected_ = false;
    if (!autoSwitch_ || count_ == 0) return;

    // Try next network in round-robin; poll() will drive the timeout
    connecting_     = true;
    connectStartUs_ = esp_timer_get_time();
}

void WifiManager::poll() {
    if (!connecting_ || !autoSwitch_ || count_ == 0) return;
    if (!connected_ &&
        esp_timer_get_time() - connectStartUs_ > kConnectTimeoutUs) {
        // Current network timed out — move to next
        int next = (currentIdx_ + 1) % count_;
        ESP_LOGI(TAG, "Timeout on '%s', trying '%s'",
                 networks_[currentIdx_].ssid, networks_[next].ssid);
        esp_wifi_disconnect();
        startConnect(next);
    }
}

// ── Network list ──────────────────────────────────────────────────────────────

bool WifiManager::addNetwork(const char* ssid, const char* pass) {
    if (count_ >= kMaxWifiNetworks) return false;
    strncpy(networks_[count_].ssid, ssid, 32); networks_[count_].ssid[32] = '\0';
    strncpy(networks_[count_].pass, pass, 64); networks_[count_].pass[64] = '\0';
    count_++;
    saveNvs();
    if (!connected_) startConnect(count_ - 1);
    return true;
}

bool WifiManager::removeNetwork(int idx) {
    if (idx < 0 || idx >= count_) return false;
    for (int i = idx; i < count_ - 1; i++) networks_[i] = networks_[i+1];
    count_--;
    saveNvs();
    if (currentIdx_ == idx) {
        connected_  = false;
        currentIdx_ = -1;
        if (count_ > 0) startConnect(0);
    }
    return true;
}

void WifiManager::setAutoSwitch(bool en) {
    autoSwitch_ = en;
    saveNvs();
}

const char* WifiManager::currentSsid() const {
    if (currentIdx_ < 0 || currentIdx_ >= count_) return "";
    return networks_[currentIdx_].ssid;
}
