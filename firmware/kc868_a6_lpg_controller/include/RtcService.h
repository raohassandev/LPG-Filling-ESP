#pragma once

#include <Arduino.h>
#include <Wire.h>

// DS3231 RTC I2C address
static constexpr uint8_t kRtcAddress = 0x68;

// Time structure
struct RtcTime
{
    uint8_t second = 0;
    uint8_t minute = 0;
    uint8_t hour = 0;
    uint8_t day = 0;  // Day of week (1-7)
    uint8_t date = 0; // Date (1-31)
    uint8_t month = 0;
    uint16_t year = 0;
};

class RtcService
{
public:
    void begin();
    void poll();

    // Time operations
    RtcTime getTime();
    void setTime(const RtcTime &time);

    // String formatting
    String getTimeString();
    String getDateString();
    String getIso8601String();

    // Status
    bool initialized() const { return rtcInitialized_; }
    bool lostPower() const { return lostPower_; }

private:
    bool rtcInitialized_ = false;
    bool lostPower_ = false;

    // BCD conversion helpers
    uint8_t bcdToDec(uint8_t bcd);
    uint8_t decToBcd(uint8_t dec);

    // Read/write RTC registers
    bool readRegisters(uint8_t startAddr, uint8_t *buffer, uint8_t length);
    bool writeRegisters(uint8_t startAddr, const uint8_t *buffer, uint8_t length);
};