#include "ResourceMonitor.h"

#include <WiFi.h>
#include <esp_system.h>

namespace {

constexpr float kNominalLoopPeriodMs = 20.0f;

uint16_t buildModeValue() {
#if defined(LPG_DEV_BUILD) && LPG_DEV_BUILD
  return 2;
#elif defined(LPG_PROTOTYPE_BUILD) && LPG_PROTOTYPE_BUILD
  return 1;
#else
  return 0;
#endif
}

float percent(uint32_t freeBytes, uint32_t totalBytes) {
  if (totalBytes == 0) return 0.0f;
  return (static_cast<float>(freeBytes) * 100.0f) / static_cast<float>(totalBytes);
}

float chipTemperatureC() {
#if defined(ESP32)
  return temperatureRead();
#else
  return NAN;
#endif
}

}  // namespace

ResourceMonitor& ResourceMonitor::instance() {
  static ResourceMonitor monitor;
  return monitor;
}

void ResourceMonitor::begin() {
  snap_.lastResetReason = static_cast<uint16_t>(esp_reset_reason());
  snap_.firmwareBuildMode = buildModeValue();
  sample(static_cast<uint16_t>(WiFi.status()), 0, 0);
}

void ResourceMonitor::sample(uint16_t wifiStatus, int16_t wifiRssiDbm, int16_t mqttClientState) {
  snap_.heapTotalBytes = ESP.getHeapSize();
  snap_.heapFreeBytes = ESP.getFreeHeap();
  snap_.heapMinFreeBytes = ESP.getMinFreeHeap();
  snap_.heapFreePercent = percent(snap_.heapFreeBytes, snap_.heapTotalBytes);
  snap_.psramTotalBytes = ESP.getPsramSize();
  snap_.psramFreeBytes = ESP.getFreePsram();
  snap_.psramFreePercent = percent(snap_.psramFreeBytes, snap_.psramTotalBytes);
  snap_.flashSizeBytes = ESP.getFlashChipSize();
  snap_.sketchSizeBytes = ESP.getSketchSize();
  snap_.freeSketchBytes = ESP.getFreeSketchSpace();
  snap_.chipTemperatureC = chipTemperatureC();
  snap_.wifiStatus = wifiStatus;
  snap_.wifiRssiDbm = wifiRssiDbm;
  snap_.mqttClientState = mqttClientState;
}

void ResourceMonitor::recordLoop(uint32_t elapsedUs) {
  const float elapsedMs = static_cast<float>(elapsedUs) / 1000.0f;
  if (!loopInitialized_) {
    snap_.mainLoopAverageMs = elapsedMs;
    snap_.mainLoopMaximumMs = elapsedMs;
    loopInitialized_ = true;
  } else {
    snap_.mainLoopAverageMs = (snap_.mainLoopAverageMs * 0.98f) + (elapsedMs * 0.02f);
    if (elapsedMs > snap_.mainLoopMaximumMs) snap_.mainLoopMaximumMs = elapsedMs;
  }
  snap_.applicationLoadPercent = snap_.mainLoopAverageMs * 100.0f / kNominalLoopPeriodMs;
  if (snap_.applicationLoadPercent > 100.0f) snap_.applicationLoadPercent = 100.0f;
  snap_.heartbeatCounter++;
}

void ResourceMonitor::incrementRtuRequest() {
  snap_.modbusRtuRequestCount++;
}

void ResourceMonitor::incrementRtuError() {
  snap_.modbusRtuErrorCount++;
}

ResourceSnapshot ResourceMonitor::snapshot() const {
  return snap_;
}
