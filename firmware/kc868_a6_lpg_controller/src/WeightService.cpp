#include "WeightService.h"

void WeightService::begin() {}

void WeightService::poll() {}

float WeightService::liveWeightKg() const { return simulatedWeightKg_; }

void WeightService::setSimulatedWeightKg(float value) { simulatedWeightKg_ = value; }

bool WeightService::stable() const { return true; }
