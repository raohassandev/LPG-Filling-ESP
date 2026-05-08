#include "screens/FillProgressScreen.h"
#include "DisplayFormat.h"
#include "Theme.h"
#include "UiHelpers.h"
#include "ScreenManager.h"

extern ScreenManager screenManager;

static const char* fillStateLabel(FillState s) {
  switch (s) {
    case FillState::Validating: return "VALIDATING";
    case FillState::Fast:       return LV_SYMBOL_PLAY "  FAST FILL";
    case FillState::Slow:       return LV_SYMBOL_PLAY "  SLOW FILL";
    case FillState::Settling:   return LV_SYMBOL_REFRESH "  SETTLING";
    default:                    return "FILLING";
  }
}

static lv_color_t fillStateColor(FillState s) {
  switch (s) {
    case FillState::Fast:     return TC::active();
    case FillState::Slow:     return TC::warning();
    case FillState::Settling: return TC::settling();
    default:                  return TC::muted();
  }
}

void FillProgressScreen::build(ModbusClient& mbus) {
  mbus_ = &mbus;
  scr_  = lv_obj_create(nullptr);
  Theme::applyScreenBg(scr_);

  // Header
  lv_obj_t* hdr = Theme::headerBar(scr_);

  lblState_ = lv_label_create(hdr);
  lv_obj_set_style_text_font(lblState_, TF::lg(), 0);
  lv_obj_set_style_text_color(lblState_, TC::active(), 0);
  lv_obj_align(lblState_, LV_ALIGN_LEFT_MID, 0, 0);

  lblTime_ = lv_label_create(hdr);
  lv_obj_set_style_text_font(lblTime_, TF::md(), 0);
  lv_obj_set_style_text_color(lblTime_, TC::textSub(), 0);
  lv_obj_align(lblTime_, LV_ALIGN_RIGHT_MID, 0, 0);

  // Main card
  lv_obj_t* card = lv_obj_create(scr_);
  lv_obj_set_size(card, 720, 330);
  lv_obj_set_pos(card, 40, 68);
  Theme::applyCard(card);

  // Net weight (hero)
  lblNet_ = lv_label_create(card);
  lv_label_set_text(lblNet_, "0.000 kg");
  lv_obj_set_style_text_font(lblNet_, TF::hero(), 0);
  lv_obj_set_style_text_color(lblNet_, TC::text(), 0);
  lv_obj_align(lblNet_, LV_ALIGN_TOP_MID, 0, 8);

  // Progress bar
  bar_ = lv_bar_create(card);
  lv_obj_set_size(bar_, 640, 28);
  lv_obj_align(bar_, LV_ALIGN_TOP_MID, 0, 90);
  lv_bar_set_range(bar_, 0, 1000);
  lv_bar_set_value(bar_, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_, TC::surface2(), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_, TC::active(), LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar_, 6, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_, 6, LV_PART_INDICATOR);

  lblPct_ = lv_label_create(card);
  lv_obj_set_style_text_font(lblPct_, TF::md(), 0);
  lv_obj_set_style_text_color(lblPct_, TC::textSub(), 0);
  lv_obj_align(lblPct_, LV_ALIGN_TOP_MID, 0, 126);

  lblTarget_ = lv_label_create(card);
  lv_obj_set_style_text_font(lblTarget_, TF::lg(), 0);
  lv_obj_set_style_text_color(lblTarget_, TC::textSub(), 0);
  lv_obj_align(lblTarget_, LV_ALIGN_TOP_MID, 0, 154);

  lblStatus_ = lv_label_create(card);
  lv_obj_set_width(lblStatus_, 640);
  lv_label_set_long_mode(lblStatus_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(lblStatus_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(lblStatus_, TF::md(), 0);
  lv_obj_set_style_text_color(lblStatus_, TC::textSub(), 0);
  lv_obj_align(lblStatus_, LV_ALIGN_TOP_MID, 0, 190);

  // Rate / amount row
  lv_obj_t* infoRow = lv_obj_create(card);
  lv_obj_set_size(infoRow, LV_PCT(100), 48);
  lv_obj_align(infoRow, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(infoRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(infoRow, 0, 0);
  lv_obj_set_style_pad_all(infoRow, 0, 0);
  lv_obj_set_layout(infoRow, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(infoRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(infoRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                         LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);

  lblRate_ = lv_label_create(infoRow);
  lv_obj_set_style_text_font(lblRate_, TF::lg(), 0);
  lv_obj_set_style_text_color(lblRate_, TC::textSub(), 0);

  lblAmount_ = lv_label_create(infoRow);
  lv_obj_set_style_text_font(lblAmount_, TF::lg(), 0);
  lv_obj_set_style_text_color(lblAmount_, TC::ready(), 0);

  // Stop button
  btnStop_ = Theme::button(scr_, "STOP FILL", TC::danger(), TC::white(), 220, 56);
  lv_obj_align(btnStop_, LV_ALIGN_BOTTOM_MID, 0, -12);
  lv_obj_add_event_cb(btnStop_, onStopPressed, LV_EVENT_CLICKED, this);
}

void FillProgressScreen::update(const ControllerSnapshot& snap) {
  if (!scr_) return;

  static FillState lastState = FillState::Idle;
  static int lastPct1000 = -1;
  static float lastNet = 1000000.0f;
  static float lastTarget = 1000000.0f;
  static float lastRate = 1000000.0f;
  static float lastCurrentAmount = 1000000.0f;
  static float lastTargetAmount = 1000000.0f;
  static uint16_t lastAlarmCode = 65535;
  static CommHealth lastCommHealth = CommHealth::Offline;
  static uint16_t lastMinute = 999;
  static uint16_t lastDay = 999;

  if (snap.state != lastState) {
    ui_label_set_text_if_changed(lblState_, fillStateLabel(snap.state));
    lv_obj_set_style_text_color(lblState_, fillStateColor(snap.state), 0);
    lv_obj_set_style_bg_color(bar_, fillStateColor(snap.state), LV_PART_INDICATOR);
    lastState = snap.state;
  }

  if (snap.rtcMinute != lastMinute || snap.rtcDay != lastDay) {
    char dt[32];
    formatRtcHeader(snap, dt, sizeof(dt));
    ui_label_set_text_if_changed(lblTime_, dt);
    lastMinute = snap.rtcMinute;
    lastDay = snap.rtcDay;
  }

  if (ui_changed_by(snap.netWeightKg, lastNet, 0.005f)) {
    ui_label_set_fmt_if_changed(lblNet_, "%.3f kg", sanitizeDisplayKg(snap.netWeightKg));
    lastNet = snap.netWeightKg;
  }

  // Progress 0–1000 (mapped from net/target)
  int pct1000 = 0;
  if (snap.targetWeightKg > 0.01f) {
    float pct = snap.netWeightKg / snap.targetWeightKg;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    pct1000 = (int)(pct * 1000);
  }
  if (pct1000 != lastPct1000) {
    lv_bar_set_value(bar_, pct1000, LV_ANIM_OFF);
    ui_label_set_fmt_if_changed(lblPct_, "%.1f%%", pct1000 / 10.0f);
    lastPct1000 = pct1000;
  }
  if (ui_changed_by(snap.targetWeightKg, lastTarget, 0.005f)) {
    ui_label_set_fmt_if_changed(lblTarget_, "Target: %.3f kg", sanitizeDisplayKg(snap.targetWeightKg));
    lastTarget = snap.targetWeightKg;
  }
  if (ui_changed_by(snap.ratePerKg, lastRate, 0.01f)) {
    ui_label_set_fmt_if_changed(lblRate_, "Rate: %.2f PKR/kg", snap.ratePerKg);
    lastRate = snap.ratePerKg;
  }
  if (ui_changed_by(snap.currentAmount, lastCurrentAmount, 0.50f) ||
      ui_changed_by(snap.targetAmount, lastTargetAmount, 0.50f)) {
    ui_label_set_fmt_if_changed(lblAmount_, "%.2f / %.2f PKR",
                                snap.currentAmount, snap.targetAmount);
    lastCurrentAmount = snap.currentAmount;
    lastTargetAmount = snap.targetAmount;
  }
  if (snap.alarmCode != lastAlarmCode || snap.commHealth != lastCommHealth) {
    if (snap.commHealth == CommHealth::Offline || !snap.connected) {
      ui_label_set_text_if_changed(lblStatus_, "Controller offline. Check RS485.");
      lv_obj_set_style_text_color(lblStatus_, TC::danger(), 0);
    } else if (snap.commHealth == CommHealth::Unstable) {
      ui_label_set_text_if_changed(lblStatus_, "RS485 unstable. Fill continues under monitoring.");
      lv_obj_set_style_text_color(lblStatus_, TC::warning(), 0);
    } else if (snap.alarmCode != 0) {
      ui_label_set_text_if_changed(lblStatus_, alarmTitle(snap.alarmCode));
      lv_obj_set_style_text_color(lblStatus_, snap.alarmSeverity >= 3 ? TC::danger() : TC::warning(), 0);
    } else {
      ui_label_set_text_if_changed(lblStatus_, "Fill in progress");
      lv_obj_set_style_text_color(lblStatus_, TC::textSub(), 0);
    }
    lastAlarmCode = snap.alarmCode;
    lastCommHealth = snap.commHealth;
  }
}

void FillProgressScreen::onStopPressed(lv_event_t* e) {
  FillProgressScreen* self = static_cast<FillProgressScreen*>(lv_event_get_user_data(e));

  // Confirm modal
  static const char* btns[] = {"STOP", "CANCEL", ""};
  lv_obj_t* mbox = lv_msgbox_create(lv_scr_act(), "Confirm Stop",
                                     "Stop the current fill?", btns, false);
  lv_obj_set_style_bg_color(mbox, TC::surface(), 0);
  lv_obj_set_style_text_color(mbox, TC::text(), 0);
  lv_obj_center(mbox);
  lv_obj_add_event_cb(mbox, onConfirmStop, LV_EVENT_VALUE_CHANGED, self);
}

void FillProgressScreen::onConfirmStop(lv_event_t* e) {
  FillProgressScreen* self = static_cast<FillProgressScreen*>(lv_event_get_user_data(e));
  lv_obj_t* mbox = lv_event_get_current_target(e);
  const char* btn = lv_msgbox_get_active_btn_text(mbox);
  if (btn && strcmp(btn, "STOP") == 0 && self->mbus_) {
    self->mbus_->cmdStop();
  }
  lv_msgbox_close(mbox);
}

void FillProgressScreen::onCancelStop(lv_event_t* e) {
  lv_obj_t* mbox = lv_event_get_current_target(e);
  lv_msgbox_close(mbox);
}
