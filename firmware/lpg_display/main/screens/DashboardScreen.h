#pragma once
#include <lvgl.h>
#include "ModbusClient.h"

class DashboardScreen {
public:
  void build(ModbusClient& mbus);
  void update(const ControllerSnapshot& snap);
  lv_obj_t* screen() { return scr_; }

private:
  lv_obj_t* scr_            = nullptr;
  lv_obj_t* stateBadge_     = nullptr;
  lv_obj_t* lblState_       = nullptr;
  lv_obj_t* weightAccent_   = nullptr;
  lv_obj_t* lblTime_        = nullptr;
  lv_obj_t* lblLive_        = nullptr;
  lv_obj_t* lblTare_        = nullptr;
  lv_obj_t* lblNet_         = nullptr;
  lv_obj_t* dotEstop_       = nullptr;
  lv_obj_t* dotCylinder_    = nullptr;
  lv_obj_t* dotNozzle_      = nullptr;
  lv_obj_t* dotStable_      = nullptr;
  lv_obj_t* lblTodayFills_  = nullptr;
  lv_obj_t* lblTodayKg_     = nullptr;
  lv_obj_t* lblTodayAmt_    = nullptr;
  lv_obj_t* btnStart_       = nullptr;
  lv_obj_t* lblConnStatus_  = nullptr;

  // Start dialog (stepper-based, no keyboard)
  lv_obj_t* startModal_     = nullptr;
  lv_obj_t* lblDialogTarget_= nullptr;
  lv_obj_t* lblDialogRate_  = nullptr;
  lv_obj_t* lblStartError_  = nullptr;
  float     dialogTargetKg_  = 12.0f;
  float     dialogRatePerKg_ = 250.0f;

  float              lastRatePerKg_ = 250.0f;
  ControllerSnapshot lastSnap_      = {};
  bool               firstUpdate_   = true;

  ModbusClient* mbus_ = nullptr;

  void updateDot(lv_obj_t* row, bool ok);
  void openStartDialog();
  void closeStartDialog();
  void updateDialogLabels();

  static void onStartPressed(lv_event_t* e);
  static void onStartConfirm(lv_event_t* e);
  static void onStartCancel(lv_event_t* e);
  static void onTargetMinus(lv_event_t* e);
  static void onTargetPlus(lv_event_t* e);
  static void onRateMinus(lv_event_t* e);
  static void onRatePlus(lv_event_t* e);
  static void onAdminPressed(lv_event_t* e);
  static void onWifiPressed(lv_event_t* e);
};
