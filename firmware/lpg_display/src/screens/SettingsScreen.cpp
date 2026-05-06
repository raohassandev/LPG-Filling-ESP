#include "screens/SettingsScreen.h"
#include "Theme.h"
#include "ScreenManager.h"

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
  lv_obj_t* tabAbout = lv_tabview_add_tab(tabview_, "ABOUT");

  buildCalibrationTab(tabCal);
  buildDiagnosticsTab(tabDiag);
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
    lv_label_set_text_fmt(lblLiveCal_,  "Live: %.3f kg", snap.liveWeightKg);
  if (lblNetCal_)
    lv_label_set_text_fmt(lblNetCal_,   "Net: %.3f kg",  snap.netWeightKg);
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
    lv_label_set_text_fmt(lblDiagWeights_,
      "Weights:  Live=%.3f  Tare=%.3f  Net=%.3f  Target=%.3f kg",
      snap.liveWeightKg, snap.tareWeightKg, snap.netWeightKg, snap.targetWeightKg);
  if (lblDiagStats_)
    lv_label_set_text_fmt(lblDiagStats_,
      "Today:  %u fills  %.2f kg  %.2f " LV_SYMBOL_CHARGE,
      snap.todayFills, snap.todayKg, snap.todayAmount);
}

void SettingsScreen::onTare(lv_event_t* e) {
  SettingsScreen* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
  if (self->mbus_) self->mbus_->writeRegister(0x0017, 5); // ZeroLive (tare)
}

void SettingsScreen::onZeroNet(lv_event_t* e) {
  SettingsScreen* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
  if (self->mbus_) self->mbus_->cmdZeroNet();
}

void SettingsScreen::onBack(lv_event_t* e) {
  screenManager.navigateTo(Screen::Dashboard);
}
