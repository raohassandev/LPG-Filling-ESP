#include "WeightService.h"

#include <Preferences.h>

void WeightService::begin()
{
    Preferences prefs;
    prefs.begin("weight", true);
    calibrationFactor_ = prefs.getFloat("cal", calibrationFactor_);
    calibrationValid_  = prefs.getBool("cal_valid", false);
    if (prefs.getBool("cal2_on", false)) {
        calLow_.rawAbs  = prefs.getLong("cal2_lr", 0);
        calLow_.kg      = prefs.getFloat("cal2_lk", 0.0f);
        calHigh_.rawAbs = prefs.getLong("cal2_hr", 0);
        calHigh_.kg     = prefs.getFloat("cal2_hk", 0.0f);
        hasTwoPoints_   = (calLow_.rawAbs != calHigh_.rawAbs) && (calLow_.kg != calHigh_.kg);
    }
    prefs.end();

    // Older firmware stored the calibration factor but not the cal_valid flag.
    // Treat any finite non-zero factor, or a valid two-point calibration, as calibrated.
    if (!calibrationValid_ && (hasTwoPoints_ || (isfinite(calibrationFactor_) && calibrationFactor_ != 0.0f))) {
        calibrationValid_ = true;
        Preferences writePrefs;
        writePrefs.begin("weight", false);
        writePrefs.putBool("cal_valid", true);
        writePrefs.end();
        Serial.println("[WEIGHT] Calibration validity migrated from saved calibration data");
    }
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
        readError_ = true;
        Serial.println("[WEIGHT] HX711 initialization failed — scale hardware fault");
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
        readError_ = true;
        return;
    }

    // Nothing to do until HX711 DOUT is ready (normal between conversions at 10/80 SPS)
    if (!dataReady()) return;

    long rawValue = 0;
    if (!readRawFast(rawValue))
    {
        // DOUT went high before we could read — not a hard error, try next cycle
        return;
    }

    // Non-blocking tare: collect this sample into the tare accumulator
    if (tareState_ == TareState::Collecting)
    {
        tareSumAcc_ += rawValue;
        tareSampleCount_++;
        if (tareSampleCount_ >= kTareSamples)
        {
            tareOffsetRaw_ = tareSumAcc_ / kTareSamples;
            liveWeightKg_  = 0.0f;
            clearStabilityHistory(0.0f);
            readError_    = false;
            tareState_     = TareState::Idle;
            Serial.printf("[WEIGHT] Non-blocking tare complete: raw offset = %ld\n", tareOffsetRaw_);
        }
        return;
    }

    applyRawSample(rawValue);
}

void WeightService::applyRawSample(long rawValue)
{
    readError_    = false;
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

    liveWeightKg_ = weightKg;
    isStable_     = checkStability(weightKg);
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
    return readRawFast(value);
}

bool WeightService::readRawFast(long& value)
{
    // Non-blocking read — caller must ensure dataReady() was true before calling.
    // If DOUT has gone high in the meantime, return false immediately.
    if (!dataReady()) return false;

    long result = 0;
    for (int i = 0; i < 24; i++)
    {
        digitalWrite(kHx711SckPin, HIGH);
        delayMicroseconds(1);
        result = (result << 1) | digitalRead(kHx711DoutPin);
        digitalWrite(kHx711SckPin, LOW);
        delayMicroseconds(1);
    }

    // Channel A, gain 128 select pulse
    digitalWrite(kHx711SckPin, HIGH);
    delayMicroseconds(1);
    digitalWrite(kHx711SckPin, LOW);
    delayMicroseconds(1);

    if (result & 0x800000) result |= 0xFF000000;

    value = result;
    return true;
}

float WeightService::liveWeightKg() const
{
    if (simActive_) return simulatedWeightKg_;
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
    // Blocking tare — only safe to call during setup() before Modbus starts,
    // or from serial console where a brief pause is acceptable.
    if (!hx711Initialized_)
    {
        return;
    }

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

void WeightService::requestTare()
{
    if (!hx711Initialized_) return;
    tareSumAcc_      = 0;
    tareSampleCount_ = 0;
    tareState_       = TareState::Collecting;
    Serial.printf("[WEIGHT] Non-blocking tare started (collecting %u samples)\n", kTareSamples);
}

bool WeightService::isTaring() const
{
    return tareState_ == TareState::Collecting;
}

void WeightService::setCalibrationFactor(float factor)
{
    calibrationFactor_ = factor;
    calibrationValid_  = true;
    Preferences prefs;
    prefs.begin("weight", false);
    prefs.putFloat("cal", factor);
    prefs.putBool("cal_valid", true);
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

    if (hasTwoPoints_) calibrationValid_ = true;
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
    prefs.putLong("cal2_hr",   calHigh_.rawAbs);
    prefs.putFloat("cal2_hk",  calHigh_.kg);
    prefs.putBool("cal2_on",   hasTwoPoints_);
    if (hasTwoPoints_) prefs.putBool("cal_valid", true);
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
