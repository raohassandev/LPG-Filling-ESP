#pragma once

#include <Arduino.h>
#include <WebServer.h>

// Manufacturing PIN service.
// PIN is stored in NVS ("lpgmfg"/"pin"). If absent at first boot, a 6-digit
// random PIN is generated and printed once to the serial boot log.
// Optionally override at compile time: -DLPG_FACTORY_MFG_PIN=123456
class MfgPinService {
public:
    void begin();

    // Verify a PIN string. Returns true if correct.
    // Wrong attempts are rate-limited (max 5 per 60 s).
    bool verify(const String& pin);

    // Extract PIN from WebServer request and verify.
    // Checks X-MFG-PIN header, then JSON body field "pin".
    // Sends 401 JSON error and returns false on failure.
    bool requirePin(WebServer& server);

    // Return the active PIN (for serial diagnostic only — never log to HTTP).
    String pin() const { return pin_; }

private:
    String       pin_;
    uint8_t      failCount_   = 0;
    unsigned long failWindowMs_ = 0;
    static constexpr uint8_t  kMaxFails   = 5;
    static constexpr uint32_t kWindowMs   = 60000UL;
};
