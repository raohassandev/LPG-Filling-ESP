#pragma once
#include <lvgl.h>
#include "ModbusClient.h"

class FillCompleteScreen {
public:
  void build(ModbusClient& mbus);
  void update(const ControllerSnapshot& snap);
  lv_obj_t* screen() { return scr_; }

private:
  lv_obj_t* scr_       = nullptr;
  lv_obj_t* lblNet_    = nullptr;
  lv_obj_t* lblAmt_    = nullptr;
  lv_obj_t* lblRate_   = nullptr;
  lv_obj_t* lblTime_   = nullptr;

  unsigned long arrivedAt_ = 0;

  ModbusClient* mbus_ = nullptr;

  static void onNewFill(lv_event_t* e);
  static void onDone(lv_event_t* e);
};
