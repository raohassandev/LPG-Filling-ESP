#pragma once
#include <lvgl.h>
#include "WifiManager.h"

class WifiScreen {
public:
    void build(WifiManager& mgr);
    void update(WifiManager& mgr);
    lv_obj_t* screen() { return scr_; }

private:
    lv_obj_t* scr_          = nullptr;
    lv_obj_t* lblStatus_    = nullptr;
    lv_obj_t* lblIp_        = nullptr;
    lv_obj_t* networkList_  = nullptr;
    lv_obj_t* swHotspot_    = nullptr;
    lv_obj_t* swAutoSwitch_ = nullptr;
    lv_obj_t* addModal_     = nullptr;
    lv_obj_t* taSsid_       = nullptr;
    lv_obj_t* taPass_       = nullptr;
    lv_obj_t* kb_           = nullptr;
    lv_obj_t* lblAddErr_    = nullptr;

    WifiManager* mgr_ = nullptr;

    // Last-seen state to skip unnecessary redraws
    bool lastConnected_  = false;
    int  lastCount_      = -1;
    bool lastAp_         = false;
    bool lastAutoSwitch_ = false;

    void rebuildNetworkList();
    void openAddModal();
    void closeAddModal();

    static void onBack(lv_event_t* e);
    static void onAddNetwork(lv_event_t* e);
    static void onAddConfirm(lv_event_t* e);
    static void onAddCancel(lv_event_t* e);
    static void onRemoveNetwork(lv_event_t* e);
    static void onHotspotToggle(lv_event_t* e);
    static void onAutoSwitchToggle(lv_event_t* e);
    static void onKeyboard(lv_event_t* e);
};
