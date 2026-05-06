#include "screens/PinScreen.h"
#include "Theme.h"
#include "ScreenManager.h"
#include "DisplayConfig.h"

extern ScreenManager screenManager;

static constexpr uint8_t kMaxDigits = 4;
static const char* kNumLabels[] = {"1","2","3","4","5","6","7","8","9","","0",LV_SYMBOL_BACKSPACE};

void PinScreen::build() {
  scr_ = lv_obj_create(nullptr);
  Theme::applyScreenBg(scr_);

  // Title
  lv_obj_t* title = lv_label_create(scr_);
  lv_label_set_text(title, "LPG FILLING STATION");
  lv_obj_set_style_text_font(title, TF::lg(), 0);
  lv_obj_set_style_text_color(title, TC::text(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, TS::xl);

  lv_obj_t* sub = lv_label_create(scr_);
  lv_label_set_text(sub, "Enter Admin PIN");
  lv_obj_set_style_text_font(sub, TF::md(), 0);
  lv_obj_set_style_text_color(sub, TC::textSub(), 0);
  lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 58);

  // PIN dots
  lblDots_ = lv_label_create(scr_);
  lv_label_set_text(lblDots_, "_ _ _ _");
  lv_obj_set_style_text_font(lblDots_, TF::xxl(), 0);
  lv_obj_set_style_text_color(lblDots_, TC::active(), 0);
  lv_obj_align(lblDots_, LV_ALIGN_TOP_MID, 0, 100);

  // Error label
  lblError_ = lv_label_create(scr_);
  lv_label_set_text(lblError_, "");
  lv_obj_set_style_text_font(lblError_, TF::md(), 0);
  lv_obj_set_style_text_color(lblError_, TC::danger(), 0);
  lv_obj_align(lblError_, LV_ALIGN_TOP_MID, 0, 148);

  // Numpad (3×4 grid, centered)
  lv_obj_t* pad = lv_obj_create(scr_);
  lv_obj_set_size(pad, 300, 280);
  lv_obj_align(pad, LV_ALIGN_CENTER, 0, 50);
  lv_obj_set_style_bg_opa(pad, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(pad, 0, 0);
  lv_obj_set_style_pad_all(pad, 0, 0);
  lv_obj_set_layout(pad, LV_LAYOUT_GRID);
  static lv_coord_t cols[] = {90, 90, 90, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t rows[] = {60, 60, 60, 60, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(pad, cols, rows);

  for (int i = 0; i < 12; i++) {
    lv_obj_t* btn = lv_btn_create(pad);
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, i % 3, 1,
                              LV_GRID_ALIGN_STRETCH, i / 3, 1);
    lv_obj_set_style_bg_color(btn, TC::surface2(), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 4, 0);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, kNumLabels[i]);
    lv_obj_set_style_text_font(lbl, TF::xl(), 0);
    lv_obj_set_style_text_color(lbl, TC::text(), 0);
    lv_obj_center(lbl);

    // key 9 (index 9) is blank spacer — no callback
    if (i == 9) {
      lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
      continue;
    }
    // key 11 = backspace
    if (i == 11) {
      lv_obj_add_event_cb(btn, onDel, LV_EVENT_CLICKED, this);
    } else {
      // digits: 1-9 → i+1, 10 → 0
      lv_obj_set_user_data(btn, (void*)(uintptr_t)((i == 10) ? 0 : i + 1));
      lv_obj_add_event_cb(btn, onKey, LV_EVENT_CLICKED, this);
    }
  }

  // Back button
  lv_obj_t* btnBack = Theme::button(scr_, LV_SYMBOL_LEFT " BACK", TC::surface2(), TC::textSub(), 120, 44);
  lv_obj_align(btnBack, LV_ALIGN_BOTTOM_LEFT, TS::xl, -TS::xl);
  lv_obj_add_event_cb(btnBack, onBack, LV_EVENT_CLICKED, this);
}

void PinScreen::appendDigit(uint8_t d) {
  if (digits_ >= kMaxDigits) return;
  entered_ = entered_ * 10 + d;
  digits_++;

  // Build dot string
  char buf[16] = "";
  for (int i = 0; i < kMaxDigits; i++) {
    if (i < digits_) strcat(buf, "● ");
    else             strcat(buf, "○ ");
  }
  buf[strlen(buf) - 1] = 0; // trim trailing space
  lv_label_set_text(lblDots_, buf);
  lv_label_set_text(lblError_, "");

  if (digits_ == kMaxDigits) submit();
}

void PinScreen::backspace() {
  if (digits_ == 0) return;
  entered_ /= 10;
  digits_--;

  char buf[16] = "";
  for (int i = 0; i < kMaxDigits; i++) {
    if (i < digits_) strcat(buf, "● ");
    else             strcat(buf, "○ ");
  }
  buf[strlen(buf) - 1] = 0;
  lv_label_set_text(lblDots_, buf);
}

void PinScreen::submit() {
  if (entered_ == kAdminPin) {
    screenManager.navigateTo(Screen::Settings);
  } else {
    lv_label_set_text(lblError_, "Incorrect PIN — try again");
    reset();
  }
}

void PinScreen::reset() {
  entered_ = 0;
  digits_  = 0;
  lv_label_set_text(lblDots_, "○ ○ ○ ○");
}

void PinScreen::onKey(lv_event_t* e) {
  PinScreen* self = static_cast<PinScreen*>(lv_event_get_user_data(e));
  lv_obj_t* btn   = lv_event_get_current_target(e);
  uint8_t   d     = (uint8_t)(uintptr_t)lv_obj_get_user_data(btn);
  self->appendDigit(d);
}

void PinScreen::onDel(lv_event_t* e) {
  static_cast<PinScreen*>(lv_event_get_user_data(e))->backspace();
}

void PinScreen::onBack(lv_event_t* e) {
  screenManager.navigateTo(Screen::Dashboard);
}

void PinScreen::onSubmit(lv_event_t* e) {
  static_cast<PinScreen*>(lv_event_get_user_data(e))->submit();
}
