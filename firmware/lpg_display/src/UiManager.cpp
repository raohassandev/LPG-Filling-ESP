#include "UiManager.h"
#include <Arduino.h>

// TODO: replace Serial stubs with LVGL screen calls once BSP is integrated.

void UiManager::begin() {
  // TODO: lv_init(); BSP display + touch init
  Serial.println("[UI] UiManager ready (stub — LVGL not yet wired)");
}

void UiManager::update(const ControllerSnapshot& snap) {
  if (!snap.valid) return;
  if (snap.stateCode == lastStateCode_) {
    // Same screen — refresh values only (no full redraw)
  }
  lastStateCode_ = snap.stateCode;

  switch (snap.stateCode) {
    case 0:  showIdle(snap);     break;
    case 1:  showFilling(snap);  break;
    case 2:  showComplete(snap); break;
    case 3:  showFault(snap);    break;
    default: showIdle(snap);     break;
  }
}

void UiManager::showIdle(const ControllerSnapshot& snap) {
  // TODO: render Idle screen
  Serial.printf("[UI] IDLE  live=%.3f kg\n", snap.liveWeightKg);
}

void UiManager::showFilling(const ControllerSnapshot& snap) {
  // TODO: render Fill Progress screen
  Serial.printf("[UI] FILL  net=%.3f / target=%.3f kg\n",
                snap.netWeightKg, snap.targetWeightKg);
}

void UiManager::showComplete(const ControllerSnapshot& snap) {
  // TODO: render Fill Complete screen
  Serial.printf("[UI] COMPLETE  net=%.3f kg  amount=%.2f\n",
                snap.netWeightKg, snap.netWeightKg * snap.ratePerKg);
}

void UiManager::showFault(const ControllerSnapshot& snap) {
  // TODO: render Fault screen
  Serial.printf("[UI] FAULT  state=%u\n", snap.stateCode);
}
