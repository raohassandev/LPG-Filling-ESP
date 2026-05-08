#include "DisplaySettings.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "DSET";
static const char* NS = "display_cfg";

static void ensureNvsReady() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(err);
}

void DisplaySettingsStore::sanitize(DisplaySettingsSnapshot& settings) {
    settings.schemaVersion = 1;

    if (settings.rtu.slaveAddress < 1 || settings.rtu.slaveAddress > 247) {
        settings.rtu.slaveAddress = 1;
    }
    switch (settings.rtu.baudRate) {
        case 1200:
        case 2400:
        case 4800:
        case 9600:
        case 19200:
        case 38400:
        case 57600:
        case 115200:
            break;
        default:
            settings.rtu.baudRate = 9600;
            break;
    }
    if (settings.rtu.parity > 2) settings.rtu.parity = 0;
    if (settings.rtu.stopBits != 1 && settings.rtu.stopBits != 2) settings.rtu.stopBits = 1;
    if (settings.rtu.timeoutMs < 150 || settings.rtu.timeoutMs > 2000) settings.rtu.timeoutMs = 300;
    if (settings.rtu.retries > 5) settings.rtu.retries = 2;
    if (settings.rtu.unstableDebounceMs < 200 || settings.rtu.unstableDebounceMs > 10000) {
        settings.rtu.unstableDebounceMs = 1500;
    }
    if (settings.rtu.offlineDebounceMs < 1000 || settings.rtu.offlineDebounceMs > 30000) {
        settings.rtu.offlineDebounceMs = 5000;
    }

    if (settings.identity.displayId[0] == '\0') {
        strlcpy(settings.identity.displayId, "DSP-001", sizeof(settings.identity.displayId));
    }
    if (settings.identity.stationId[0] == '\0') {
        strlcpy(settings.identity.stationId, "LPG-STN-001", sizeof(settings.identity.stationId));
    }
}

DisplaySettingsSnapshot DisplaySettingsStore::load() {
    ensureNvsReady();

    DisplaySettingsSnapshot settings;
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        sanitize(settings);
        return settings;
    }

    uint16_t schema = settings.schemaVersion;
    nvs_get_u16(h, "schema", &schema);
    settings.schemaVersion = schema;

    nvs_get_u8(h, "slave", &settings.rtu.slaveAddress);
    nvs_get_u32(h, "baud", &settings.rtu.baudRate);
    nvs_get_u8(h, "parity", &settings.rtu.parity);
    nvs_get_u8(h, "stop", &settings.rtu.stopBits);
    nvs_get_u16(h, "timeout", &settings.rtu.timeoutMs);
    nvs_get_u8(h, "retries", &settings.rtu.retries);
    nvs_get_u16(h, "unstable", &settings.rtu.unstableDebounceMs);
    nvs_get_u16(h, "offline", &settings.rtu.offlineDebounceMs);

    size_t len = sizeof(settings.identity.displayId);
    nvs_get_str(h, "display", settings.identity.displayId, &len);
    len = sizeof(settings.identity.stationId);
    nvs_get_str(h, "station", settings.identity.stationId, &len);

    nvs_close(h);
    sanitize(settings);
    ESP_LOGI(TAG, "Loaded RTU slave=%u baud=%lu parity=%u stop=%u timeout=%u retries=%u",
             settings.rtu.slaveAddress, static_cast<unsigned long>(settings.rtu.baudRate),
             settings.rtu.parity, settings.rtu.stopBits, settings.rtu.timeoutMs,
             settings.rtu.retries);
    return settings;
}

bool DisplaySettingsStore::save(const DisplaySettingsSnapshot& source) {
    ensureNvsReady();

    DisplaySettingsSnapshot settings = source;
    sanitize(settings);

    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;

    bool ok = true;
    ok &= nvs_set_u16(h, "schema", settings.schemaVersion) == ESP_OK;
    ok &= nvs_set_u8(h, "slave", settings.rtu.slaveAddress) == ESP_OK;
    ok &= nvs_set_u32(h, "baud", settings.rtu.baudRate) == ESP_OK;
    ok &= nvs_set_u8(h, "parity", settings.rtu.parity) == ESP_OK;
    ok &= nvs_set_u8(h, "stop", settings.rtu.stopBits) == ESP_OK;
    ok &= nvs_set_u16(h, "timeout", settings.rtu.timeoutMs) == ESP_OK;
    ok &= nvs_set_u8(h, "retries", settings.rtu.retries) == ESP_OK;
    ok &= nvs_set_u16(h, "unstable", settings.rtu.unstableDebounceMs) == ESP_OK;
    ok &= nvs_set_u16(h, "offline", settings.rtu.offlineDebounceMs) == ESP_OK;
    ok &= nvs_set_str(h, "display", settings.identity.displayId) == ESP_OK;
    ok &= nvs_set_str(h, "station", settings.identity.stationId) == ESP_OK;
    ok &= nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}
