#include "screens/FillCompleteScreen.h"
#include "DisplayFormat.h"
#include "Theme.h"
#include "UiHelpers.h"
#include "ScreenManager.h"
#include "esp_timer.h"

extern ScreenManager screenManager;

void FillCompleteScreen::build(ModbusClient& mbus) {
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

  // Checkmark icon
  lv_obj_t* icon = lv_obj_create(scr_);
  lv_obj_set_size(icon, 80, 80);
  lv_obj_set_pos(icon, 360, 72);
  lv_obj_set_style_bg_color(icon, TC::ready(), 0);
  lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(icon, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(icon, 0, 0);
  lv_obj_t* iconLbl = lv_label_create(icon);
  lv_label_set_text(iconLbl, LV_SYMBOL_OK);
  lv_obj_set_style_text_font(iconLbl, TF::xxl(), 0);
  lv_obj_set_style_text_color(iconLbl, TC::white(), 0);
  lv_obj_center(iconLbl);

  // COMPLETE title
  lv_obj_t* title = lv_label_create(scr_);
  lv_label_set_text(title, "FILL COMPLETE");
  lv_obj_set_style_text_font(title, TF::xxl(), 0);
  lv_obj_set_style_text_color(title, TC::ready(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 164);

  // Summary card
  lv_obj_t* card = lv_obj_create(scr_);
  lv_obj_set_size(card, 480, 160);
  lv_obj_set_pos(card, 160, 210);
  Theme::applyCard(card, TC::surface());

  lv_obj_t* grid = lv_obj_create(card);
  lv_obj_set_size(grid, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_EVENLY,
                         LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  auto makeRow = [](lv_obj_t* parent, const char* key, lv_obj_t** valueOut) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);

    lv_obj_t* k = lv_label_create(row);
    lv_label_set_text(k, key);
    lv_obj_set_style_text_font(k, TF::md(), 0);
    lv_obj_set_style_text_color(k, TC::textSub(), 0);
    lv_obj_align(k, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* v = lv_label_create(row);
    lv_obj_set_style_text_font(v, TF::lg(), 0);
    lv_obj_set_style_text_color(v, TC::text(), 0);
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    *valueOut = v;
  };

  makeRow(grid, "Net Weight",  &lblNet_);
  makeRow(grid, "Amount",      &lblAmt_);
  makeRow(grid, "Rate",        &lblRate_);

  // Buttons
  lv_obj_t* btnNew = Theme::button(scr_, "NEW FILL", TC::active(), TC::white(), 200, 56);
  lv_obj_set_pos(btnNew, 180, 400);
  lv_obj_add_event_cb(btnNew, onNewFill, LV_EVENT_CLICKED, this);

  lv_obj_t* btnDone = Theme::button(scr_, "DONE", TC::surface2(), TC::text(), 200, 56);
  lv_obj_set_pos(btnDone, 420, 400);
  lv_obj_add_event_cb(btnDone, onDone, LV_EVENT_CLICKED, this);

  arrivedAt_ = (uint32_t)(esp_timer_get_time() / 1000);
}

void FillCompleteScreen::update(const ControllerSnapshot& snap) {
  if (!scr_) return;
  char dt[32];
  formatRtcHeader(snap, dt, sizeof(dt));
  ui_label_set_text_if_changed(lblTime_, dt);
  display_label_setf(lblNet_,  "%.3f kg", sanitizeDisplayKg(snap.netWeightKg));
  display_label_setf(lblAmt_,  "%.2f PKR", snap.currentAmount);
  display_label_setf(lblRate_, "%.2f PKR/kg", snap.ratePerKg);
}

void FillCompleteScreen::onNewFill(lv_event_t* e) {
  FillCompleteScreen* self = static_cast<FillCompleteScreen*>(lv_event_get_user_data(e));
  if (self->mbus_) self->mbus_->cmdReset();
  screenManager.navigateTo(Screen::Dashboard);
}

void FillCompleteScreen::onDone(lv_event_t* e) {
  FillCompleteScreen* self = static_cast<FillCompleteScreen*>(lv_event_get_user_data(e));
  if (self->mbus_) self->mbus_->cmdReset();
  screenManager.navigateTo(Screen::Dashboard);
}
