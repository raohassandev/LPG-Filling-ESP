#include "ScreenManager.h"
#include "screens/DashboardScreen.h"
#include "screens/FillProgressScreen.h"
#include "screens/FillCompleteScreen.h"
#include "screens/FaultScreen.h"
#include "screens/PinScreen.h"
#include "screens/SettingsScreen.h"

// ── Singleton screen instances ────────────────────────────────────────────────
static DashboardScreen    dashScr;
static FillProgressScreen progScr;
static FillCompleteScreen compScr;
static FaultScreen        faultScr;
static PinScreen          pinScr;
static SettingsScreen     settScr;

void ScreenManager::begin() {
  // Screens are lazily built on first navigate — nothing to do here
}

Screen ScreenManager::stateToScreen(FillState state) const {
  switch (state) {
    case FillState::Validating:
    case FillState::Fast:
    case FillState::Slow:
    case FillState::Settling:    return Screen::FillProgress;
    case FillState::Complete:    return Screen::FillComplete;
    case FillState::Fault:
    case FillState::Aborted:     return Screen::Fault;
    default:                     return Screen::Dashboard;
  }
}

void ScreenManager::update(const ControllerSnapshot& snap, ModbusClient& mbus) {
  if (!snap.valid) return;

  // Auto-transition based on controller state (only for operational screens,
  // not for user-initiated screens like Pin / Settings)
  if (current_ != Screen::Pin && current_ != Screen::Settings) {
    Screen target = stateToScreen(snap.state);
    if (target != current_) {
      loadScreen(target, snap, mbus);
    }
  }

  // Refresh current screen data
  switch (current_) {
    case Screen::Dashboard:   dashScr.update(snap);  break;
    case Screen::FillProgress:progScr.update(snap);  break;
    case Screen::FillComplete:compScr.update(snap);  break;
    case Screen::Fault:       faultScr.update(snap); break;
    case Screen::Settings:    settScr.update(snap);  break;
    default: break;
  }
}

void ScreenManager::navigateTo(Screen s) {
  // Will be applied on next update() call
  pending_       = s;
  transitioning_ = true;

  // For immediate user-driven nav we apply right away (no snapshot needed)
  if (s == Screen::Dashboard || s == Screen::Pin || s == Screen::Settings) {
    ControllerSnapshot dummy;
    ModbusClient* dummy_mbus = nullptr;
    // Build screens that don't need a snapshot immediately
    switch (s) {
      case Screen::Pin:
        if (!pinScr.screen()) pinScr.build();
        lv_scr_load_anim(pinScr.screen(), LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
        current_ = s;
        break;
      case Screen::Settings:
        // Settings needs mbus — deferred to update()
        pending_ = s;
        break;
      default:
        pending_ = s;
        break;
    }
  }
}

void ScreenManager::loadScreen(Screen s, const ControllerSnapshot& snap, ModbusClient& mbus) {
  lv_obj_t* next = nullptr;

  switch (s) {
    case Screen::Dashboard:
      if (!dashScr.screen()) dashScr.build(mbus);
      dashScr.update(snap);
      next = dashScr.screen();
      break;
    case Screen::FillProgress:
      if (!progScr.screen()) progScr.build(mbus);
      progScr.update(snap);
      next = progScr.screen();
      break;
    case Screen::FillComplete:
      if (!compScr.screen()) compScr.build(mbus);
      compScr.update(snap);
      next = compScr.screen();
      break;
    case Screen::Fault:
      if (!faultScr.screen()) faultScr.build(mbus);
      faultScr.update(snap);
      next = faultScr.screen();
      break;
    case Screen::Pin:
      if (!pinScr.screen()) pinScr.build();
      next = pinScr.screen();
      break;
    case Screen::Settings:
      if (!settScr.screen()) settScr.build(mbus);
      settScr.update(snap);
      next = settScr.screen();
      break;
  }

  if (next && next != lv_scr_act()) {
    lv_scr_load_anim(next, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
  }

  current_       = s;
  transitioning_ = false;
}
