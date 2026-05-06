#pragma once
#include <lvgl.h>

class PinScreen {
public:
  void build();
  lv_obj_t* screen() { return scr_; }

private:
  lv_obj_t* scr_       = nullptr;
  lv_obj_t* lblDots_   = nullptr;
  lv_obj_t* lblError_  = nullptr;

  uint32_t entered_ = 0;
  uint8_t  digits_  = 0;

  void appendDigit(uint8_t d);
  void backspace();
  void submit();
  void reset();

  static void onKey(lv_event_t* e);
  static void onBack(lv_event_t* e);
  static void onDel(lv_event_t* e);
  static void onSubmit(lv_event_t* e);
};
