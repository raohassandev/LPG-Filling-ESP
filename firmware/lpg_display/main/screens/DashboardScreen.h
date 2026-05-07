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
  lv_obj_t* lblMbus_        = nullptr;
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
  lv_obj_t* lblActionIcon_   = nullptr;
  lv_obj_t* lblActionText_   = nullptr;
  lv_obj_t* alertCard_      = nullptr;
  lv_obj_t* lblAlertTitle_  = nullptr;
  lv_obj_t* lblAlertBody_   = nullptr;
  lv_obj_t* offlineModal_   = nullptr;

  // Fill params strip
  lv_obj_t* fpCellTarget_ = nullptr;
  lv_obj_t* fpCellRate_   = nullptr;
  lv_obj_t* fpCellAmount_ = nullptr;
  lv_obj_t* lblTarget_    = nullptr;
  lv_obj_t* lblRate_      = nullptr;
  lv_obj_t* lblAmount_    = nullptr;

  // Start dialog (stepper-based, no keyboard)
  lv_obj_t* startModal_     = nullptr;
  lv_obj_t* lblDialogTarget_= nullptr;
  lv_obj_t* lblDialogRate_  = nullptr;
  lv_obj_t* lblStartError_  = nullptr;
  float     dialogTargetKg_  = 12.0f;
  float     dialogRatePerKg_ = 250.0f;

  // Numeric input overlay
  lv_obj_t* numOverlay_   = nullptr;
  lv_obj_t* numTa_        = nullptr;
  lv_obj_t* numKb_        = nullptr;
  lv_obj_t* numHint_      = nullptr;
  uint8_t   numField_     = 0;  // 0=target, 1=rate

  float              lastRatePerKg_ = 250.0f;
  ControllerSnapshot lastSnap_      = {};
  bool               firstUpdate_   = true;
  bool               stableBlinking_= false;

  // Role selector
  enum class Role : uint8_t { Operator = 0, Admin, Manufacturer };
  Role         currentRole_    = Role::Operator;
  Role         pendingRole_    = Role::Operator;
  lv_obj_t*    btnRole_        = nullptr;
  lv_obj_t*    lblRoleBtn_     = nullptr;
  lv_obj_t*    roleModal_      = nullptr;
  lv_obj_t*    lblRolePinDots_ = nullptr;
  lv_obj_t*    lblRolePinErr_  = nullptr;
  uint32_t     roleEntered_    = 0;
  uint8_t      roleDigits_     = 0;

  ModbusClient* mbus_ = nullptr;

  void updateDot(lv_obj_t* row, bool ok);
  void openStartDialog();
  void closeStartDialog();
  void openNumOverlay(uint8_t field);
  void closeNumOverlay();
  void updateDialogLabels();
  void startStableBlink();
  void stopStableBlink();
  void updateRoleButton();
  void openRoleModal();
  void closeRoleModal();
  void buildRoleChooser();
  void buildRolePinEntry();
  void appendRoleDigit(uint8_t d);
  void submitRolePin();
  void openOfflineModal();
  void closeOfflineModal();
  void updateAlert(const ControllerSnapshot& snap, bool fullRefresh);

  static void onStartPressed(lv_event_t* e);
  static void onStartConfirm(lv_event_t* e);
  static void onStartCancel(lv_event_t* e);
  static void onTargetMinus(lv_event_t* e);
  static void onTargetPlus(lv_event_t* e);
  static void onRateMinus(lv_event_t* e);
  static void onRatePlus(lv_event_t* e);
  static void onFpCellTapped(lv_event_t* e);
  static void onNumKbEvent(lv_event_t* e);
  static void onOfflineOk(lv_event_t* e);
  static void onWifiPressed(lv_event_t* e);
  static void onRolePressed(lv_event_t* e);
  static void onRolePinKey(lv_event_t* e);
  static void onRolePinDel(lv_event_t* e);
  static void onRoleSelect(lv_event_t* e);
  static void blinkAnimCb(void* obj, int32_t v);
};
