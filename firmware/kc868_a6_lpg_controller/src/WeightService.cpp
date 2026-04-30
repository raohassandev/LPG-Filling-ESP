#include "WeightService.h"

void WeightService::begin()
{
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

        float weightKg = static_cast<float>(rawValue - tareOffsetRaw_) / calibrationFactor_;

        // Update history and check stability
        liveWeightKg_ = weightKg;
        isStable_ = checkStability(weightKg);
    }
    else
    {
        readError_ = true;
    }
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
}

void WeightService::setSimulatedWeightKg(float weightKg)
{
    simulatedWeightKg_ = weightKg;
    if (!hx711Initialized_)
    {
        liveWeightKg_ = weightKg;
        readError_ = false;
        isStable_ = true;
    }
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

    // Stability threshold: variance < 0.01 (10g variance)
    return variance < 0.01f;
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
