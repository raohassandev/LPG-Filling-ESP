#pragma once
#include <lvgl.h>
#include "ModbusClient.h"

class DashboardScreen {
public:
  void build(ModbusClient& mbus);
  void update(const ControllerSnapshot& snap);
  lv_obj_t* screen() { return scr_; }

private:
  lv_obj_t* scr_           = nullptr;
  lv_obj_t* lblState_      = nullptr;
  lv_obj_t* lblTime_       = nullptr;
  lv_obj_t* lblLive_       = nullptr;
  lv_obj_t* lblTare_       = nullptr;
  lv_obj_t* lblNet_        = nullptr;
  lv_obj_t* dotEstop_      = nullptr;
  lv_obj_t* dotCylinder_   = nullptr;
  lv_obj_t* dotNozzle_     = nullptr;
  lv_obj_t* dotStable_     = nullptr;
  lv_obj_t* lblTodayFills_ = nullptr;
  lv_obj_t* lblTodayKg_    = nullptr;
  lv_obj_t* lblTodayAmt_   = nullptr;
  lv_obj_t* btnStart_      = nullptr;
  lv_obj_t* lblConnStatus_ = nullptr;

  ModbusClient* mbus_ = nullptr;

  void updateDot(lv_obj_t* dot, bool ok);
  static void onStartPressed(lv_event_t* e);
  static void onAdminPressed(lv_event_t* e);
};
