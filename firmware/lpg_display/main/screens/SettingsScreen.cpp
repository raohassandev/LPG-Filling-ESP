#include "screens/SettingsScreen.h"
#include "DisplaySettings.h"
#include "DisplayFormat.h"
#include "Theme.h"
#include "ScreenManager.h"
#include "UiHelpers.h"
#include <stdlib.h>
#include <initializer_list>

extern ScreenManager screenManager;

void SettingsScreen::build(ModbusClient& mbus) {
  mbus_ = &mbus;
  scr_  = lv_obj_create(nullptr);
  Theme::applyScreenBg(scr_);

  // Header
  lv_obj_t* hdr = Theme::headerBar(scr_);
  Theme::label(hdr, "ADMIN SETTINGS", TF::lg(), TC::text());
  lv_obj_t* hdrTitle = lv_obj_get_child(hdr, 0);
  lv_obj_align(hdrTitle, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t* btnBack = Theme::button(hdr, LV_SYMBOL_LEFT " BACK", TC::surface2(), TC::textSub(), 110, 36);
  lv_obj_align(btnBack, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(btnBack, onBack, LV_EVENT_CLICKED, this);

  // Tab view
  tabview_ = lv_tabview_create(scr_, LV_DIR_TOP, 48);
  lv_obj_set_size(tabview_, 800, 428);
  lv_obj_set_pos(tabview_, 0, 52);
  lv_obj_set_style_bg_color(tabview_, TC::bg(), 0);
  lv_obj_set_style_bg_color(lv_tabview_get_tab_btns(tabview_), TC::surface(), 0);

  lv_obj_t* tabCal   = lv_tabview_add_tab(tabview_, "CALIBRATION");
  lv_obj_t* tabDiag  = lv_tabview_add_tab(tabview_, "DIAGNOSTICS");
  lv_obj_t* tabLink  = lv_tabview_add_tab(tabview_, "CONTROLLER LINK");
  lv_obj_t* tabAbout = lv_tabview_add_tab(tabview_, "ABOUT");

  buildCalibrationTab(tabCal);
  buildDiagnosticsTab(tabDiag);
  buildControllerLinkTab(tabLink);
  buildAboutTab(tabAbout);
}

void SettingsScreen::buildCalibrationTab(lv_obj_t* tab) {
  lv_obj_set_style_bg_color(tab, TC::bg(), 0);

  // Live weight display
  lv_obj_t* card1 = lv_obj_create(tab);
  lv_obj_set_size(card1, LV_PCT(100), 100);
  lv_obj_set_pos(card1, 0, 0);
  Theme::applyCard(card1);

  Theme::label(card1, "CURRENT READINGS", TF::sm(), TC::textSub());
  lv_obj_t* hdr = lv_obj_get_child(card1, 0);
  lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t* row = lv_obj_create(card1);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_align(row, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_layout(row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                         LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);

  lblLiveCal_  = lv_label_create(row);
  lblNetCal_   = lv_label_create(row);
  lblStableCal_= lv_label_create(row);
  for (lv_obj_t* l : {lblLiveCal_, lblNetCal_, lblStableCal_}) {
    lv_obj_set_style_text_font(l, TF::lg(), 0);
    lv_obj_set_style_text_color(l, TC::text(), 0);
  }

  // Action buttons
  lv_obj_t* btnTare = Theme::button(tab, "TARE SCALE", TC::active(), TC::white(), 200, 52);
  lv_obj_set_pos(btnTare, 0, 120);
  lv_obj_add_event_cb(btnTare, onTare, LV_EVENT_CLICKED, this);

  lv_obj_t* btnZero = Theme::button(tab, "ZERO NET", TC::warning(), TC::black(), 200, 52);
  lv_obj_set_pos(btnZero, 220, 120);
  lv_obj_add_event_cb(btnZero, onZeroNet, LV_EVENT_CLICKED, this);
}

void SettingsScreen::buildDiagnosticsTab(lv_obj_t* tab) {
  lv_obj_set_style_bg_color(tab, TC::bg(), 0);

  lv_obj_t* card = lv_obj_create(tab);
  lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
  Theme::applyCard(card);
  lv_obj_set_layout(card, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START,
                         LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(card, TS::md, 0);

  auto addRow = [&](lv_obj_t** out) {
    *out = lv_label_create(card);
    lv_obj_set_style_text_font(*out, TF::md(), 0);
    lv_obj_set_style_text_color(*out, TC::text(), 0);
  };

  addRow(&lblDiagState_);
  addRow(&lblDiagFlags_);
  addRow(&lblDiagWeights_);
  addRow(&lblDiagStats_);
}

static lv_obj_t* makeSmallInput(lv_obj_t* parent, const char* label, const char* value, int x, int y, int w) {
  lv_obj_t* lbl = Theme::label(parent, label, TF::sm(), TC::textSub());
  lv_obj_set_pos(lbl, x, y);
  lv_obj_t* ta = lv_textarea_create(parent);
  lv_obj_set_size(ta, w, 42);
  lv_obj_set_pos(ta, x, y + 22);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_text(ta, value);
  lv_obj_set_style_text_font(ta, TF::md(), 0);
  lv_obj_set_style_bg_color(ta, TC::surface2(), 0);
  lv_obj_set_style_text_color(ta, TC::text(), 0);
  lv_obj_set_style_border_color(ta, TC::border(), 0);
  return ta;
}

void SettingsScreen::buildControllerLinkTab(lv_obj_t* tab) {
  lv_obj_set_style_bg_color(tab, TC::bg(), 0);

  lv_obj_t* card = lv_obj_create(tab);
  lv_obj_set_size(card, LV_PCT(100), 312);
  lv_obj_set_pos(card, 0, 0);
  Theme::applyCard(card);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lblLinkStatus_ = Theme::label(card, "RS485: OFFLINE", TF::lg(), TC::danger());
  lv_obj_align(lblLinkStatus_, LV_ALIGN_TOP_LEFT, 0, 0);

  const DisplayRtuSettings rtu = mbus_ ? mbus_->rtuSettings() : DisplaySettingsStore::load().rtu;
  char buf[24];
  snprintf(buf, sizeof(buf), "%u", rtu.slaveAddress);
  taSlave_ = makeSmallInput(card, "Slave", buf, 0, 46, 110);
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(rtu.baudRate));
  taBaud_ = makeSmallInput(card, "Baud", buf, 130, 46, 130);
  snprintf(buf, sizeof(buf), "%u", rtu.timeoutMs);
  taTimeout_ = makeSmallInput(card, "Timeout ms", buf, 280, 46, 140);
  snprintf(buf, sizeof(buf), "%u", rtu.retries);
  taRetries_ = makeSmallInput(card, "Retries", buf, 440, 46, 110);
  snprintf(buf, sizeof(buf), "%u", rtu.unstableDebounceMs);
  taUnstable_ = makeSmallInput(card, "Unstable ms", buf, 570, 46, 150);

  lv_obj_t* lblParity = Theme::label(card, "Parity", TF::sm(), TC::textSub());
  lv_obj_set_pos(lblParity, 0, 130);
  ddParity_ = lv_dropdown_create(card);
  lv_dropdown_set_options(ddParity_, "None\nEven\nOdd");
  lv_dropdown_set_selected(ddParity_, rtu.parity <= 2 ? rtu.parity : 0);
  lv_obj_set_size(ddParity_, 160, 42);
  lv_obj_set_pos(ddParity_, 0, 152);

  lv_obj_t* lblStop = Theme::label(card, "Stop Bits", TF::sm(), TC::textSub());
  lv_obj_set_pos(lblStop, 180, 130);
  ddStopBits_ = lv_dropdown_create(card);
  lv_dropdown_set_options(ddStopBits_, "1\n2");
  lv_dropdown_set_selected(ddStopBits_, rtu.stopBits == 2 ? 1 : 0);
  lv_obj_set_size(ddStopBits_, 120, 42);
  lv_obj_set_pos(ddStopBits_, 180, 152);

  snprintf(buf, sizeof(buf), "%u", rtu.offlineDebounceMs);
  taOffline_ = makeSmallInput(card, "Offline ms", buf, 320, 130, 150);

  lv_obj_t* btnSave = Theme::button(card, LV_SYMBOL_SAVE " SAVE & RECONNECT", TC::active(), TC::white(), 230, 46);
  lv_obj_set_pos(btnSave, 0, 222);
  lv_obj_add_event_cb(btnSave, onSaveLink, LV_EVENT_CLICKED, this);

  lv_obj_t* btnTest = Theme::button(card, LV_SYMBOL_REFRESH " TEST DEVICE ID", TC::surface2(), TC::text(), 210, 46);
  lv_obj_set_pos(btnTest, 250, 222);
  lv_obj_add_event_cb(btnTest, onTestLink, LV_EVENT_CLICKED, this);

  lblLinkMsg_ = Theme::label(card, "Saved defaults match current controller: slave 1, 9600 8N1.", TF::md(), TC::textSub());
  lv_obj_set_width(lblLinkMsg_, 700);
  lv_label_set_long_mode(lblLinkMsg_, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(lblLinkMsg_, 0, 276);
}

void SettingsScreen::buildAboutTab(lv_obj_t* tab) {
  lv_obj_set_style_bg_color(tab, TC::bg(), 0);

  lv_obj_t* card = lv_obj_create(tab);
  lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
  Theme::applyCard(card);

  lblAbout_ = lv_label_create(card);
  lv_label_set_text(lblAbout_,
    "LPG Display Firmware\n"
    "Board: ESP32-S3 5\" Capacitive Touch\n"
    "Resolution: 800 × 480\n"
    "Interface: Modbus RTU RS485\n"
    "Controller: KC868-A6 (lpg_controller)\n\n"
    "Theme: Dark Industrial\n"
    "UI: LVGL 8.x");
  lv_obj_set_style_text_font(lblAbout_, TF::md(), 0);
  lv_obj_set_style_text_color(lblAbout_, TC::text(), 0);
}

void SettingsScreen::update(const ControllerSnapshot& snap) {
  if (!scr_) return;

  // Calibration tab
  if (lblLiveCal_)
    display_label_setf(lblLiveCal_,  "Live: %.3f kg", snap.liveWeightKg);
  if (lblNetCal_)
    display_label_setf(lblNetCal_,   "Net: %.3f kg",  snap.netWeightKg);
  if (lblStableCal_)
    lv_label_set_text(lblStableCal_, snap.weightStable ? "STABLE" : "SETTLING");

  // Diagnostics tab
  if (lblDiagState_)
    lv_label_set_text_fmt(lblDiagState_, "State:  %u", (uint8_t)snap.state);
  if (lblDiagFlags_)
    lv_label_set_text_fmt(lblDiagFlags_,
      "Flags:  E-Stop=%u  Cylinder=%u  Nozzle=%u  Stable=%u",
      snap.eStopOk, snap.cylinderPresent, snap.nozzleEngaged, snap.weightStable);
  if (lblDiagWeights_)
    display_label_setf(lblDiagWeights_,
      "Weights:  Live=%.3f  Tare=%.3f  Net=%.3f  Target=%.3f kg",
      snap.liveWeightKg, snap.tareWeightKg, snap.netWeightKg, snap.targetWeightKg);
  if (lblDiagStats_)
    display_label_setf(lblDiagStats_,
      "Today:  %u fills  %.2f kg  %.2f " LV_SYMBOL_CHARGE,
      snap.todayFills, snap.todayKg, snap.todayAmount);

  if (lblLinkStatus_) {
    const char* text = "RS485: OFFLINE";
    lv_color_t color = TC::danger();
    if (snap.commHealth == CommHealth::Online) {
      text = "RS485: ONLINE";
      color = TC::ready();
    } else if (snap.commHealth == CommHealth::Unstable) {
      text = "RS485: UNSTABLE";
      color = TC::warning();
    }
    ui_label_set_text_if_changed(lblLinkStatus_, text);
    lv_obj_set_style_text_color(lblLinkStatus_, color, 0);
  }
}

void SettingsScreen::onTare(lv_event_t* e) {
  SettingsScreen* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
  // The public Modbus command set exposes Zero Net, not hardware tare.
  if (self->mbus_) self->mbus_->cmdZeroNet();
}

void SettingsScreen::onZeroNet(lv_event_t* e) {
  SettingsScreen* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
  if (self->mbus_) self->mbus_->cmdZeroNet();
}

void SettingsScreen::onSaveLink(lv_event_t* e) {
  SettingsScreen* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
  if (!self || !self->mbus_) return;

  DisplaySettingsSnapshot settings = DisplaySettingsStore::load();
  settings.rtu.slaveAddress = static_cast<uint8_t>(atoi(lv_textarea_get_text(self->taSlave_)));
  settings.rtu.baudRate = static_cast<uint32_t>(strtoul(lv_textarea_get_text(self->taBaud_), nullptr, 10));
  settings.rtu.timeoutMs = static_cast<uint16_t>(atoi(lv_textarea_get_text(self->taTimeout_)));
  settings.rtu.retries = static_cast<uint8_t>(atoi(lv_textarea_get_text(self->taRetries_)));
  settings.rtu.unstableDebounceMs = static_cast<uint16_t>(atoi(lv_textarea_get_text(self->taUnstable_)));
  settings.rtu.offlineDebounceMs = static_cast<uint16_t>(atoi(lv_textarea_get_text(self->taOffline_)));
  settings.rtu.parity = static_cast<uint8_t>(lv_dropdown_get_selected(self->ddParity_));
  settings.rtu.stopBits = lv_dropdown_get_selected(self->ddStopBits_) == 1 ? 2 : 1;

  if (DisplaySettingsStore::save(settings)) {
    settings = DisplaySettingsStore::load();
    self->mbus_->applySettings(settings.rtu);
    ui_label_set_text_if_changed(self->lblLinkMsg_, "RS485 settings saved. Reconnecting with new parameters.");
  } else {
    ui_label_set_text_if_changed(self->lblLinkMsg_, "Save failed. Settings were not changed.");
  }
}

void SettingsScreen::onTestLink(lv_event_t* e) {
  SettingsScreen* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
  if (!self || !self->mbus_) return;
  uint16_t deviceId = 0;
  if (self->mbus_->readDeviceId(deviceId) && deviceId == 0xA601) {
    ui_label_set_text_if_changed(self->lblLinkMsg_, "Device ID OK: controller returned 0xA601.");
  } else if (deviceId != 0) {
    lv_label_set_text_fmt(self->lblLinkMsg_, "Device ID mismatch: expected 0xA601, got 0x%04X.", deviceId);
  } else {
    ui_label_set_text_if_changed(self->lblLinkMsg_, "Device ID test failed: no RTU response. Check RS485 wiring, slave address, baud, parity, and stop bits.");
  }
}

void SettingsScreen::onBack(lv_event_t* e) {
  screenManager.navigateTo(Screen::Dashboard);
}
