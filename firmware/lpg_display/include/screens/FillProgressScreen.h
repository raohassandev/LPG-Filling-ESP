#pragma once
#include <lvgl.h>
#include "ModbusClient.h"

class FillProgressScreen {
public:
  void build(ModbusClient& mbus);
  void update(const ControllerSnapshot& snap);
  lv_obj_t* screen() { return scr_; }

private:
  lv_obj_t* scr_        = nullptr;
  lv_obj_t* lblState_   = nullptr;
  lv_obj_t* lblTime_    = nullptr;
  lv_obj_t* lblNet_     = nullptr;
  lv_obj_t* bar_        = nullptr;
  lv_obj_t* lblPct_     = nullptr;
  lv_obj_t* lblTarget_  = nullptr;
  lv_obj_t* lblRate_    = nullptr;
  lv_obj_t* lblAmount_  = nullptr;
  lv_obj_t* btnStop_    = nullptr;
  lv_obj_t* mboxStop_   = nullptr;

  ModbusClient* mbus_ = nullptr;

  static void onStopPressed(lv_event_t* e);
  static void onConfirmStop(lv_event_t* e);
  static void onCancelStop(lv_event_t* e);
};
