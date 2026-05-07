#include "ScreenManager.h"
#include "Bsp.h"
#include "screens/DashboardScreen.h"
#include "screens/FillProgressScreen.h"
#include "screens/FillCompleteScreen.h"
#include "screens/FaultScreen.h"
#include "screens/PinScreen.h"
#include "screens/SettingsScreen.h"

static DashboardScreen    dashScr;
static FillProgressScreen progScr;
static FillCompleteScreen compScr;
static FaultScreen        faultScr;
static PinScreen          pinScr;
static SettingsScreen     settScr;

void ScreenManager::begin(ModbusClient& mbus) {
    ControllerSnapshot empty{};
    if (Bsp::lock()) {
        dashScr.build(mbus);
        dashScr.update(empty);
        lv_scr_load(dashScr.screen());
        Bsp::unlock();
    }
    current_ = Screen::Dashboard;
    pending_ = Screen::Dashboard;
}

Screen ScreenManager::stateToScreen(FillState s) const {
    switch (s) {
        case FillState::Validating:
        case FillState::Fast:
        case FillState::Slow:
        case FillState::Settling:  return Screen::FillProgress;
        case FillState::Complete:  return Screen::FillComplete;
        case FillState::Fault:
        case FillState::Aborted:   return Screen::Fault;
        default:                   return Screen::Dashboard;
    }
}

void ScreenManager::update(const ControllerSnapshot& snap, ModbusClient& mbus) {
    // Always update dashboard so the connection-status label stays current.
    // For all other screens, bail if we have no valid data.
    if (!snap.valid && current_ != Screen::Dashboard) return;

    // Auto-transition on state change (not for user-driven screens)
    if (current_ != Screen::Pin && current_ != Screen::Settings) {
        Screen target = stateToScreen(snap.state);
        if (target != current_) loadScreen(target, snap, mbus);
    }

    // Handle deferred navigateTo() calls
    if (pending_ != current_) loadScreen(pending_, snap, mbus);

    // Refresh data on current screen (inside LVGL lock)
    if (Bsp::lock()) {
        switch (current_) {
            case Screen::Dashboard:    dashScr.update(snap);   break;
            case Screen::FillProgress: progScr.update(snap);   break;
            case Screen::FillComplete: compScr.update(snap);   break;
            case Screen::Fault:        faultScr.update(snap);  break;
            case Screen::Settings:     settScr.update(snap);   break;
            default: break;
        }
        Bsp::unlock();
    }
}

void ScreenManager::navigateTo(Screen s) {
    pending_ = s;
}

void ScreenManager::loadScreen(Screen s, const ControllerSnapshot& snap, ModbusClient& mbus) {
    lv_obj_t* next = nullptr;

    if (Bsp::lock()) {
        switch (s) {
            case Screen::Dashboard:
                if (!dashScr.screen()) dashScr.build(mbus);
                dashScr.update(snap);
                next = dashScr.screen(); break;
            case Screen::FillProgress:
                if (!progScr.screen()) progScr.build(mbus);
                progScr.update(snap);
                next = progScr.screen(); break;
            case Screen::FillComplete:
                if (!compScr.screen()) compScr.build(mbus);
                compScr.update(snap);
                next = compScr.screen(); break;
            case Screen::Fault:
                if (!faultScr.screen()) faultScr.build(mbus);
                faultScr.update(snap);
                next = faultScr.screen(); break;
            case Screen::Pin:
                if (!pinScr.screen()) pinScr.build();
                next = pinScr.screen(); break;
            case Screen::Settings:
                if (!settScr.screen()) settScr.build(mbus);
                settScr.update(snap);
                next = settScr.screen(); break;
        }
        if (next && next != lv_scr_act())
            lv_scr_load_anim(next, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
        Bsp::unlock();
    }

    current_ = s;
    pending_ = s;
}
