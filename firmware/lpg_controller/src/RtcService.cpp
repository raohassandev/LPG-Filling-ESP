#include "RtcService.h"

void RtcService::begin()
{
    // Check if RTC is present
    Wire.beginTransmission(kRtcAddress);
    if (Wire.endTransmission() != 0)
    {
        rtcInitialized_ = false;
        Serial.println("[RTC] DS3231 not found");
        return;
    }

    // Check oscillator stop flag (bit 7 of DS3231 status register 0x0F)
    uint8_t status;
    if (readRegisters(0x0F, &status, 1))
    {
        lostPower_ = (status & 0x80) != 0;

        // Clear oscillator stop flag if set
        if (lostPower_)
        {
            status &= 0x7F;
            writeRegisters(0x0F, &status, 1);
        }
    }

    rtcInitialized_ = true;
    Serial.println("[RTC] DS3231 initialized");

    if (lostPower_)
    {
        Serial.println("[RTC] Warning: RTC lost power - time may be invalid");
    }
}

void RtcService::poll()
{
    // RTC doesn't need polling - just read on demand
}

RtcTime RtcService::getTime()
{
    RtcTime time;

    if (!rtcInitialized_)
    {
        return time;
    }

    uint8_t data[7];
    if (!readRegisters(0x00, data, 7))
    {
        return time;
    }

    time.second = bcdToDec(data[0] & 0x7F);
    time.minute = bcdToDec(data[1] & 0x7F);
    time.hour = bcdToDec(data[2] & 0x3F);
    time.day = bcdToDec(data[3] & 0x3F);
    time.date = bcdToDec(data[4] & 0x3F);
    time.month = bcdToDec(data[5] & 0x1F);
    time.year = 2000 + bcdToDec(data[6]);

    return time;
}

void RtcService::setTime(const RtcTime &time)
{
    if (!rtcInitialized_)
    {
        return;
    }

    uint8_t data[7];
    data[0] = decToBcd(time.second);
    data[1] = decToBcd(time.minute);
    data[2] = decToBcd(time.hour);
    data[3] = decToBcd(time.day);  // Day of week
    data[4] = decToBcd(time.date); // Date
    data[5] = decToBcd(time.month);
    data[6] = decToBcd(time.year - 2000);

    if (writeRegisters(0x00, data, 7))
    {
        Serial.println("[RTC] Time set successfully");
    }
    else
    {
        Serial.println("[RTC] Failed to set time");
    }
}

String RtcService::getTimeString()
{
    RtcTime time = getTime();

    char buffer[9];
    snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u",
             time.hour, time.minute, time.second);

    return String(buffer);
}

String RtcService::getDateString()
{
    RtcTime time = getTime();

    char buffer[11];
    snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u",
             time.year, time.month, time.date);

    return String(buffer);
}

String RtcService::getIso8601String()
{
    RtcTime time = getTime();

    char buffer[21];
    snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u",
             time.year, time.month, time.date,
             time.hour, time.minute, time.second);

    return String(buffer);
}

uint8_t RtcService::bcdToDec(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

uint8_t RtcService::decToBcd(uint8_t dec)
{
    return ((dec / 10) << 4) | (dec % 10);
}

bool RtcService::readRegisters(uint8_t startAddr, uint8_t *buffer, uint8_t length)
{
    Wire.beginTransmission(kRtcAddress);
    Wire.write(startAddr);
    if (Wire.endTransmission() != 0)
    {
        return false;
    }

    if (Wire.requestFrom(kRtcAddress, length) != length)
    {
        return false;
    }

    for (uint8_t i = 0; i < length; i++)
    {
        buffer[i] = Wire.read();
    }

    return true;
}

bool RtcService::writeRegisters(uint8_t startAddr, const uint8_t *buffer, uint8_t length)
{
    Wire.beginTransmission(kRtcAddress);
    Wire.write(startAddr);

    for (uint8_t i = 0; i < length; i++)
    {
        Wire.write(buffer[i]);
    }

    return Wire.endTransmission() == 0;
}
