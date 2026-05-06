#pragma once
#include <lvgl.h>
#include "ModbusClient.h"

class FaultScreen {
public:
  void build(ModbusClient& mbus);
  void update(const ControllerSnapshot& snap);
  lv_obj_t* screen() { return scr_; }

private:
  lv_obj_t* scr_       = nullptr;
  lv_obj_t* lblTitle_  = nullptr;
  lv_obj_t* lblReason_ = nullptr;
  lv_obj_t* lblHint_   = nullptr;
  lv_obj_t* lblTime_   = nullptr;

  ModbusClient* mbus_ = nullptr;

  static void onReset(lv_event_t* e);
};
