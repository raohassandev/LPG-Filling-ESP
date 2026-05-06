#include "screens/FaultScreen.h"
#include "Theme.h"
#include "ScreenManager.h"

extern ScreenManager screenManager;

static const char* faultHint(FillState s) {
  if (s == FillState::Aborted) return "Fill was stopped by operator.";
  return "Check nozzle connection, cylinder placement,\nand e-stop status, then reset.";
}

void FaultScreen::build(ModbusClient& mbus) {
  mbus_ = &mbus;
  scr_  = lv_obj_create(nullptr);
  Theme::applyScreenBg(scr_);

  // Header
  lv_obj_t* hdr = Theme::headerBar(scr_);
  Theme::label(hdr, "LPG FILLING STATION", TF::lg(), TC::text());
  lv_obj_t* hdrTitle = lv_obj_get_child(hdr, 0);
  lv_obj_align(hdrTitle, LV_ALIGN_LEFT_MID, 0, 0);

  lblTime_ = lv_label_create(hdr);
  lv_obj_set_style_text_font(lblTime_, TF::md(), 0);
  lv_obj_set_style_text_color(lblTime_, TC::textSub(), 0);
  lv_obj_align(lblTime_, LV_ALIGN_RIGHT_MID, 0, 0);

  // Error icon
  lv_obj_t* icon = lv_obj_create(scr_);
  lv_obj_set_size(icon, 80, 80);
  lv_obj_set_pos(icon, 360, 72);
  lv_obj_set_style_bg_color(icon, TC::danger(), 0);
  lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(icon, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(icon, 0, 0);
  lv_obj_t* iconLbl = lv_label_create(icon);
  lv_label_set_text(iconLbl, LV_SYMBOL_WARNING);
  lv_obj_set_style_text_font(iconLbl, TF::xxl(), 0);
  lv_obj_set_style_text_color(iconLbl, TC::white(), 0);
  lv_obj_center(iconLbl);

  // Title
  lblTitle_ = lv_label_create(scr_);
  lv_obj_set_style_text_font(lblTitle_, TF::xxl(), 0);
  lv_obj_set_style_text_color(lblTitle_, TC::danger(), 0);
  lv_obj_align(lblTitle_, LV_ALIGN_TOP_MID, 0, 164);

  // Reason card
  lv_obj_t* card = lv_obj_create(scr_);
  lv_obj_set_size(card, 600, 130);
  lv_obj_set_pos(card, 100, 216);
  Theme::applyCard(card, TC::surface());

  lblReason_ = lv_label_create(card);
  lv_obj_set_style_text_font(lblReason_, TF::lg(), 0);
  lv_obj_set_style_text_color(lblReason_, TC::text(), 0);
  lv_obj_align(lblReason_, LV_ALIGN_TOP_MID, 0, 0);

  lblHint_ = lv_label_create(card);
  lv_obj_set_style_text_font(lblHint_, TF::md(), 0);
  lv_obj_set_style_text_color(lblHint_, TC::textSub(), 0);
  lv_label_set_long_mode(lblHint_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lblHint_, 560);
  lv_obj_align(lblHint_, LV_ALIGN_BOTTOM_MID, 0, 0);

  // Reset button
  lv_obj_t* btnReset = Theme::button(scr_, "RESET", TC::warning(), TC::black(), 200, 56);
  lv_obj_align(btnReset, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_add_event_cb(btnReset, onReset, LV_EVENT_CLICKED, this);
}

void FaultScreen::update(const ControllerSnapshot& snap) {
  if (!scr_) return;
  lv_label_set_text_fmt(lblTime_, "%02u:%02u", snap.rtcHour, snap.rtcMinute);

  if (snap.state == FillState::Aborted) {
    lv_label_set_text(lblTitle_, "ABORTED");
    lv_obj_set_style_text_color(lblTitle_, TC::warning(), 0);
  } else {
    lv_label_set_text(lblTitle_, "FAULT");
    lv_obj_set_style_text_color(lblTitle_, TC::danger(), 0);
  }

  lv_label_set_text(lblReason_, snap.state == FillState::Aborted
                                ? "Fill stopped by operator" : "System fault detected");
  lv_label_set_text(lblHint_, faultHint(snap.state));
}

void FaultScreen::onReset(lv_event_t* e) {
  FaultScreen* self = static_cast<FaultScreen*>(lv_event_get_user_data(e));
  if (self->mbus_) self->mbus_->cmdReset();
  screenManager.navigateTo(Screen::Dashboard);
}
