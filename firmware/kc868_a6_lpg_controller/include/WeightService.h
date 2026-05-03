#pragma once

#include <Arduino.h>

// HX711 direct implementation - no external library required
// Bit-bangs HX711 protocol on ESP32 GPIO

class WeightService
{
public:
  void begin();
  void poll();

  float liveWeightKg() const;
  bool stable() const;

  // Calibration methods
  void tare();
  void setCalibrationFactor(float factor);
  float calibrationFactor() const { return calibrationFactor_; }
  void setSimulatedWeightKg(float weightKg);
  void clearSimulation();
  bool simActive() const { return simActive_; }
  long lastRawValue() const { return lastRawValue_; }
  long tareOffsetRaw() const { return tareOffsetRaw_; }

  // Status
  bool initialized() const { return hx711Initialized_; }
  bool readFailed() const { return readError_; }
  uint8_t doutPin() const { return kHx711DoutPin; }
  uint8_t sckPin() const { return kHx711SckPin; }
  int doutLevel() const;
  int sckLevel() const;

private:
  // HX711 pins - KC868-A6 exposed IO terminals from the board pinout.
  static constexpr uint8_t kHx711DoutPin = 32; // HX711 DOUT/DT -> KC868-A6 IO-1 / ESP32 GPIO32
  static constexpr uint8_t kHx711SckPin = 33;  // HX711 SCK     -> KC868-A6 IO-2 / ESP32 GPIO33

  float calibrationFactor_ = -7050.0f; // Default calibration factor (needs tuning)
  long tareOffsetRaw_ = 0;
  float liveWeightKg_ = 0.0f;
  float simulatedWeightKg_ = 0.0f;
  bool simActive_ = false;
  long lastRawValue_ = 0;
  bool hx711Initialized_ = false;
  bool readError_ = false;

  // Stability detection
  static constexpr uint8_t kStabilityWindow = 10;
  float weightHistory_[kStabilityWindow] = {0};
  uint8_t historyIndex_ = 0;
  bool isStable_ = false;

  // HX711 low-level operations
  bool dataReady() const;
  long readRawHx711();
  bool readRawHx711(long& value, uint16_t timeoutMs);
  bool checkStability(float newWeight);
  void clearStabilityHistory(float value);
};
