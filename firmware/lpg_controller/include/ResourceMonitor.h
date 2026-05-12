#pragma once

#include <Arduino.h>
#include <stdint.h>

struct ResourceSnapshot {
  float applicationLoadPercent = 0.0f;
  float mainLoopAverageMs = 0.0f;
  float mainLoopMaximumMs = 0.0f;
  uint32_t heapTotalBytes = 0;
  uint32_t heapFreeBytes = 0;
  uint32_t heapMinFreeBytes = 0;
  float heapFreePercent = 0.0f;
  uint32_t psramTotalBytes = 0;
  uint32_t psramFreeBytes = 0;
  float psramFreePercent = 0.0f;
  uint32_t flashSizeBytes = 0;
  uint32_t sketchSizeBytes = 0;
  uint32_t freeSketchBytes = 0;
  float chipTemperatureC = NAN;
  int16_t wifiRssiDbm = 0;
  uint16_t wifiStatus = 0;
  int16_t mqttClientState = 0;
  uint16_t lastResetReason = 0;
  uint16_t firmwareBuildMode = 0;
  uint32_t modbusRtuRequestCount = 0;
  uint32_t modbusRtuErrorCount = 0;
  uint32_t heartbeatCounter = 0;
  uint32_t rtuLastUs = 0;
  uint32_t rtuMaxUs = 0;
  uint32_t rtuAvgUs = 0;
  uint32_t modbusTcpRequestCount = 0;
  uint32_t modbusTcpErrorCount = 0;
  uint32_t tcpLastUs = 0;
  uint32_t tcpMaxUs = 0;
  uint32_t tcpAvgUs = 0;
};

class ResourceMonitor {
public:
  static ResourceMonitor& instance();

  void begin();
  void sample(uint16_t wifiStatus, int16_t wifiRssiDbm, int16_t mqttClientState);
  void recordLoop(uint32_t elapsedUs);
  void incrementRtuRequest();
  void incrementRtuError();
  void recordRtuTiming(uint32_t us);
  void incrementTcpRequest();
  void incrementTcpError();
  void recordTcpTiming(uint32_t us);

  ResourceSnapshot snapshot() const;

private:
  ResourceMonitor() = default;

  ResourceSnapshot snap_;
  bool loopInitialized_ = false;
};
