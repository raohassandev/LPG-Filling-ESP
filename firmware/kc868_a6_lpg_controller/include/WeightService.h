#pragma once

#include <Arduino.h>

class WeightService {
 public:
  void begin();
  void poll();

  float liveWeightKg() const;
  void setSimulatedWeightKg(float value);
  bool stable() const;

 private:
  float simulatedWeightKg_ = 0.0f;
};
