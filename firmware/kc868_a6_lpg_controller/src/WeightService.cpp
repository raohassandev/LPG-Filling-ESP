#include "WeightService.h"

#include <Preferences.h>

void WeightService::begin()
{
    Preferences prefs;
    prefs.begin("weight", true);
    calibrationFactor_ = prefs.getFloat("cal", calibrationFactor_);
    if (prefs.getBool("cal2_on", false)) {
        calLow_.rawAbs  = prefs.getLong("cal2_lr", 0);
        calLow_.kg      = prefs.getFloat("cal2_lk", 0.0f);
        calHigh_.rawAbs = prefs.getLong("cal2_hr", 0);
        calHigh_.kg     = prefs.getFloat("cal2_hk", 0.0f);
        hasTwoPoints_   = (calLow_.rawAbs != calHigh_.rawAbs) && (calLow_.kg != calHigh_.kg);
    }
    prefs.end();
    if (hasTwoPoints_) {
        Serial.printf("[WEIGHT] Two-point cal loaded: low=%.3fkg@%ld  high=%.3fkg@%ld\n",
                      calLow_.kg, calLow_.rawAbs, calHigh_.kg, calHigh_.rawAbs);
    } else {
        Serial.printf("[WEIGHT] Single-point cal loaded: factor=%.2f\n", calibrationFactor_);
    }

    // Configure GPIO pins
    pinMode(kHx711DoutPin, INPUT_PULLUP);
    pinMode(kHx711SckPin, OUTPUT);

    digitalWrite(kHx711SckPin, LOW);

    // Wait for HX711 to be ready
    delay(500);

    // Check if HX711 is ready (DOUT goes low)
    if (digitalRead(kHx711DoutPin) == LOW)
    {
        hx711Initialized_ = true;
        Serial.println("[WEIGHT] HX711 initialized successfully");

        tare();
    }
    else
    {
        hx711Initialized_ = false;
        liveWeightKg_ = simulatedWeightKg_;
        Serial.println("[WEIGHT] HX711 initialization failed - using simulated weight path");
    }
}

void WeightService::poll()
{
    if (simActive_)
    {
        liveWeightKg_ = simulatedWeightKg_;
        return;
    }

    if (!hx711Initialized_)
    {
        liveWeightKg_ = simulatedWeightKg_;
        return;
    }

    if (dataReady())
    {
        long rawValue = 0;
        if (!readRawHx711(rawValue, 100))
        {
            readError_ = true;
            return;
        }
        readError_ = false;
        lastRawValue_ = rawValue;

        float weightKg;
        if (hasTwoPoints_) {
            const float span = static_cast<float>(calHigh_.rawAbs - calLow_.rawAbs);
            weightKg = (span != 0.0f)
                ? calLow_.kg + static_cast<float>(rawValue - calLow_.rawAbs) * (calHigh_.kg - calLow_.kg) / span
                : 0.0f;
        } else {
            weightKg = static_cast<float>(rawValue - tareOffsetRaw_) / calibrationFactor_;
        }

        // Update history and check stability
        liveWeightKg_ = weightKg;
        isStable_ = checkStability(weightKg);
    }
    // else: HX711 is mid-conversion (DOUT HIGH is normal at 10 SPS) — not an error
}

long WeightService::readRawHx711()
{
    long value = 0;
    if (!readRawHx711(value, 100))
    {
        return 0;
    }
    return value;
}

bool WeightService::dataReady() const
{
    return digitalRead(kHx711DoutPin) == LOW;
}

bool WeightService::readRawHx711(long& value, uint16_t timeoutMs)
{
    // Wait for data ready (DOUT goes low)
    const unsigned long startedAt = millis();
    while (!dataReady())
    {
        if (millis() - startedAt >= timeoutMs)
        {
            return false;
        }
        delay(1);
    }

    // Read 24-bit value
    long result = 0;
    for (int i = 0; i < 24; i++)
    {
        digitalWrite(kHx711SckPin, HIGH);
        delayMicroseconds(1);
        result = (result << 1) | digitalRead(kHx711DoutPin);
        digitalWrite(kHx711SckPin, LOW);
        delayMicroseconds(1);
    }

    // Send pulse for channel/gain selection (channel A, gain 128)
    for (int i = 0; i < 1; i++)
    {
        digitalWrite(kHx711SckPin, HIGH);
        delayMicroseconds(1);
        digitalWrite(kHx711SckPin, LOW);
        delayMicroseconds(1);
    }

    // Convert from unsigned to signed
    if (result & 0x800000)
    {
        result |= 0xFF000000;
    }

    value = result;
    return true;
}

float WeightService::liveWeightKg() const
{
    if (!hx711Initialized_)
    {
        return simulatedWeightKg_;
    }

    if (readError_)
    {
        return liveWeightKg_;
    }

    return liveWeightKg_;
}

bool WeightService::stable() const
{
    return isStable_;
}

int WeightService::doutLevel() const
{
    return digitalRead(kHx711DoutPin);
}

int WeightService::sckLevel() const
{
    return digitalRead(kHx711SckPin);
}

void WeightService::tare()
{
    if (!hx711Initialized_)
    {
        return;
    }

    static constexpr uint8_t kTareSamples = 15;
    long sum = 0;
    uint8_t samples = 0;
    while (samples < kTareSamples)
    {
        long rawValue = 0;
        if (readRawHx711(rawValue, 250))
        {
            sum += rawValue;
            lastRawValue_ = rawValue;
            samples++;
        }
        delay(20);
    }

    tareOffsetRaw_ = sum / kTareSamples;
    liveWeightKg_ = 0.0f;
    readError_ = false;
    clearStabilityHistory(0.0f);
    Serial.printf("[WEIGHT] Tare completed: raw offset = %ld\n", tareOffsetRaw_);
}

void WeightService::setCalibrationFactor(float factor)
{
    calibrationFactor_ = factor;
    Preferences prefs;
    prefs.begin("weight", false);
    prefs.putFloat("cal", factor);
    prefs.end();
    Serial.printf("[WEIGHT] Single-point factor saved: %.2f\n", factor);
}

void WeightService::setCalPoint(uint8_t point, float knownKg)
{
    CalPoint& pt = (point == 1) ? calLow_ : calHigh_;
    pt.rawAbs = lastRawValue_;
    pt.kg     = knownKg;

    hasTwoPoints_ = (calLow_.rawAbs != calHigh_.rawAbs)
                 && (calLow_.kg     != calHigh_.kg)
                 && !(calLow_.rawAbs == 0 && calLow_.kg == 0.0f)
                 && !(calHigh_.rawAbs == 0 && calHigh_.kg == 0.0f);

    persistCalPoints();
    Serial.printf("[WEIGHT] Cal point %d set: %.3fkg @ raw %ld  two-point=%s\n",
                  point, knownKg, pt.rawAbs, hasTwoPoints_ ? "YES" : "NO");
}

void WeightService::clearCalPoints()
{
    calLow_  = CalPoint{};
    calHigh_ = CalPoint{};
    hasTwoPoints_ = false;
    Preferences prefs;
    prefs.begin("weight", false);
    prefs.putBool("cal2_on", false);
    prefs.end();
    Serial.println("[WEIGHT] Two-point cal cleared");
}

void WeightService::persistCalPoints()
{
    Preferences prefs;
    prefs.begin("weight", false);
    prefs.putLong("cal2_lr",  calLow_.rawAbs);
    prefs.putFloat("cal2_lk", calLow_.kg);
    prefs.putLong("cal2_hr",  calHigh_.rawAbs);
    prefs.putFloat("cal2_hk", calHigh_.kg);
    prefs.putBool("cal2_on",  hasTwoPoints_);
    prefs.end();
}

void WeightService::setSimulatedWeightKg(float weightKg)
{
    simulatedWeightKg_ = weightKg;
    simActive_ = true;
    liveWeightKg_ = weightKg;
    readError_ = false;
    isStable_ = true;
}

void WeightService::clearSimulation()
{
    simActive_ = false;
    simulatedWeightKg_ = 0.0f;
    Serial.println("[WEIGHT] Simulation cleared — reading live sensor");
}

bool WeightService::checkStability(float newWeight)
{
    weightHistory_[historyIndex_] = newWeight;
    historyIndex_ = (historyIndex_ + 1) % kStabilityWindow;

    // Calculate variance
    float sum = 0;
    float sumSq = 0;
    for (uint8_t i = 0; i < kStabilityWindow; i++)
    {
        sum += weightHistory_[i];
        sumSq += weightHistory_[i] * weightHistory_[i];
    }
    float mean = sum / kStabilityWindow;
    float variance = (sumSq / kStabilityWindow) - (mean * mean);

    // Stability threshold: variance < 0.04 (~200g std-dev for 100kg cell)
    return variance < 0.04f;
}

void WeightService::clearStabilityHistory(float value)
{
    for (uint8_t i = 0; i < kStabilityWindow; i++)
    {
        weightHistory_[i] = value;
    }
    historyIndex_ = 0;
    isStable_ = true;
}
