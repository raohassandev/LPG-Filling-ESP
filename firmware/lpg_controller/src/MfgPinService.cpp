#pragma GCC optimize("Os")
#include "MfgPinService.h"
#include <Preferences.h>
#include <esp_random.h>

void MfgPinService::begin() {
    Preferences prefs;
    prefs.begin("lpgmfg", false);
    pin_ = prefs.getString("pin", "");

    if (pin_.isEmpty()) {
#if defined(LPG_FACTORY_MFG_PIN)
        pin_ = String(LPG_FACTORY_MFG_PIN);
#else
        // Generate a random 6-digit PIN (100000-999999)
        const uint32_t r = esp_random();
        pin_ = String(100000 + (r % 900000));
#endif
        prefs.putString("pin", pin_);
        Serial.printf("[MFG-PIN] *** MANUFACTURING PIN (first boot): %s ***\n", pin_.c_str());
        Serial.println(F("[MFG-PIN] Store this PIN. It will not be shown again unless NVS is erased."));
    } else {
        Serial.println(F("[MFG-PIN] Manufacturing PIN loaded from NVS."));
    }
    prefs.end();
}

bool MfgPinService::verify(const String& pin) {
    const unsigned long nowMs = millis();
    // Reset rate-limit window
    if (nowMs - failWindowMs_ > kWindowMs) {
        failCount_    = 0;
        failWindowMs_ = nowMs;
    }
    if (failCount_ >= kMaxFails) return false;

    if (pin == pin_) {
        failCount_ = 0;
        return true;
    }
    failCount_++;
    delay(200);  // brief throttle per failed attempt
    return false;
}

bool MfgPinService::requirePin(WebServer& server) {
    if (failCount_ >= kMaxFails) {
        server.send(429, "application/json",
                    F("{\"ok\":false,\"message\":\"Too many PIN attempts. Wait 60 s.\"}"));
        return false;
    }
    const String pin = server.header("X-MFG-PIN");
    if (!verify(pin)) {
        server.send(401, "application/json",
                    F("{\"ok\":false,\"message\":\"Invalid manufacturing PIN.\"}"));
        return false;
    }
    return true;
}
