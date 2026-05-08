#pragma once
#include <lvgl.h>
#include "ModbusClient.h"

class SettingsScreen {
public:
  void build(ModbusClient& mbus);
  void update(const ControllerSnapshot& snap);
  lv_obj_t* screen() { return scr_; }

private:
  lv_obj_t* scr_       = nullptr;
  lv_obj_t* tabview_   = nullptr;

  // Calibration tab
  lv_obj_t* lblLiveCal_  = nullptr;
  lv_obj_t* lblNetCal_   = nullptr;
  lv_obj_t* lblStableCal_= nullptr;

  // Diagnostics tab
  lv_obj_t* lblDiagState_   = nullptr;
  lv_obj_t* lblDiagFlags_   = nullptr;
  lv_obj_t* lblDiagWeights_ = nullptr;
  lv_obj_t* lblDiagStats_   = nullptr;

  // About tab
  lv_obj_t* lblAbout_ = nullptr;

  // Controller Link tab
  lv_obj_t* lblLinkStatus_ = nullptr;
  lv_obj_t* taSlave_ = nullptr;
  lv_obj_t* taBaud_ = nullptr;
  lv_obj_t* ddParity_ = nullptr;
  lv_obj_t* ddStopBits_ = nullptr;
  lv_obj_t* taTimeout_ = nullptr;
  lv_obj_t* taRetries_ = nullptr;
  lv_obj_t* lblLinkMsg_ = nullptr;

  ModbusClient* mbus_ = nullptr;

  void buildCalibrationTab(lv_obj_t* tab);
  void buildDiagnosticsTab(lv_obj_t* tab);
  void buildControllerLinkTab(lv_obj_t* tab);
  void buildAboutTab(lv_obj_t* tab);

  static void onTare(lv_event_t* e);
  static void onZeroNet(lv_event_t* e);
  static void onSaveLink(lv_event_t* e);
  static void onTestLink(lv_event_t* e);
  static void onBack(lv_event_t* e);
};
