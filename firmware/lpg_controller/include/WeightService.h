#pragma once

#include <Arduino.h>

// HX711 direct implementation - no external library required
// Bit-bangs HX711 protocol on ESP32 GPIO

struct CalPoint {
  long rawAbs = 0;   // absolute HX711 raw reading when knownKg was on scale
  float kg    = 0.0f;
};

class WeightService
{
public:
  void begin();

  // Non-blocking poll — call every loop iteration.
  // Reads one HX711 sample only when DOUT is ready; returns immediately otherwise.
  void poll();

  float liveWeightKg() const;
  bool stable() const;

  // Single-point calibration (legacy)
  // tare() is BLOCKING — use only from begin() or the serial console.
  void tare();

  // Non-blocking tare: start sample collection via the poll() state machine.
  // Completes when poll() has collected kTareSamples data-ready readings.
  void requestTare();
  bool isTaring() const;

  void setCalibrationFactor(float factor);
  float calibrationFactor() const { return calibrationFactor_; }

  // Two-point calibration
  // point must be 1 (low) or 2 (high). Uses lastRawValue_ at the moment of the call.
  // Both points must be set for two-point mode to activate.
  void setCalPoint(uint8_t point, float knownKg);
  void clearCalPoints();
  bool hasTwoPoints()           const { return hasTwoPoints_; }
  const CalPoint& calLowPoint() const { return calLow_; }
  const CalPoint& calHighPoint()const { return calHigh_; }

  // Simulation
  void setSimulatedWeightKg(float weightKg);
  void clearSimulation();
  bool simActive() const { return simActive_; }

  // Raw access
  long lastRawValue()  const { return lastRawValue_; }
  long tareOffsetRaw() const { return tareOffsetRaw_; }

  // Calibration validity — set true after any successful calibration action
  bool calibrationValid() const { return calibrationValid_; }
  void markCalibrationValid() { calibrationValid_ = true; }

  // Status
  bool initialized() const { return hx711Initialized_; }
  bool readFailed()  const { return readError_; }
  uint8_t doutPin()  const { return kHx711DoutPin; }
  uint8_t sckPin()   const { return kHx711SckPin; }
  int doutLevel()    const;
  int sckLevel()     const;

private:
  // HX711 pins - KC868-A6 exposed IO terminals from the board pinout.
  static constexpr uint8_t kHx711DoutPin = 32; // HX711 DOUT/DT -> KC868-A6 IO-1 / ESP32 GPIO32
  static constexpr uint8_t kHx711SckPin = 33;  // HX711 SCK     -> KC868-A6 IO-2 / ESP32 GPIO33

  // Single-point calibration (existing, used when hasTwoPoints_ == false)
  float calibrationFactor_ = -7050.0f;
  long  tareOffsetRaw_     = 0;

  // Two-point calibration
  CalPoint calLow_, calHigh_;
  bool     hasTwoPoints_ = false;

  float liveWeightKg_      = 0.0f;
  float simulatedWeightKg_ = 0.0f;
  bool  simActive_         = false;
  long  lastRawValue_      = 0;
  bool  hx711Initialized_  = false;
  bool  readError_         = false;
  bool  calibrationValid_  = false;

  // Tare / stability
  static constexpr uint8_t kTareSamples     = 15;
  static constexpr uint8_t kStabilityWindow = 10;
  float   weightHistory_[kStabilityWindow] = {0};
  uint8_t historyIndex_ = 0;
  bool    isStable_     = false;

  // HX711 low-level
  bool dataReady() const;
  long readRawHx711();
  bool readRawHx711(long& value, uint16_t timeoutMs);

  // Fast, non-blocking 24-bit read. Only call when dataReady() is true.
  // No timeout loop — returns false immediately if DOUT went high.
  bool readRawFast(long& value);

  bool checkStability(float newWeight);
  void clearStabilityHistory(float value);
  void applyRawSample(long rawValue);

  // Non-blocking tare state machine
  enum class TareState : uint8_t { Idle, Collecting };
  TareState tareState_    = TareState::Idle;
  long      tareSumAcc_   = 0;
  uint8_t   tareSampleCount_ = 0;
  void persistCalPoints();
};
