#pragma once
#include <lvgl.h>

// ── Color tokens (mirrors apps/lpg-expo-app/src/theme.js) ────────────────────
namespace TC {
  static inline lv_color_t bg()       { return lv_color_hex(0x0f1117); }
  static inline lv_color_t surface()  { return lv_color_hex(0x1a1d27); }
  static inline lv_color_t surface2() { return lv_color_hex(0x222633); }
  static inline lv_color_t border()   { return lv_color_hex(0x2a2d3a); }
  static inline lv_color_t text()     { return lv_color_hex(0xf0f2f5); }
  static inline lv_color_t textSub()  { return lv_color_hex(0x9ca3af); }
  static inline lv_color_t muted()    { return lv_color_hex(0x6b7280); }
  static inline lv_color_t ready()    { return lv_color_hex(0x10b981); }
  static inline lv_color_t warning()  { return lv_color_hex(0xf59e0b); }
  static inline lv_color_t danger()   { return lv_color_hex(0xef4444); }
  static inline lv_color_t active()   { return lv_color_hex(0x3b82f6); }
  static inline lv_color_t settling() { return lv_color_hex(0x8b5cf6); }
  static inline lv_color_t complete() { return lv_color_hex(0x10b981); }
  static inline lv_color_t white()    { return lv_color_hex(0xffffff); }
  static inline lv_color_t black()    { return lv_color_hex(0x000000); }
}

// ── Spacing ───────────────────────────────────────────────────────────────────
namespace TS {
  static constexpr int xs  = 4;
  static constexpr int sm  = 8;
  static constexpr int md  = 12;
  static constexpr int lg  = 16;
  static constexpr int xl  = 24;
  static constexpr int xxl = 32;
}

// ── Font aliases (LVGL built-in fonts) ───────────────────────────────────────
// Swap for custom fonts if desired.
namespace TF {
  static inline const lv_font_t* xs()   { return &lv_font_montserrat_12; }
  static inline const lv_font_t* sm()   { return &lv_font_montserrat_14; }
  static inline const lv_font_t* md()   { return &lv_font_montserrat_16; }
  static inline const lv_font_t* lg()   { return &lv_font_montserrat_20; }
  static inline const lv_font_t* xl()   { return &lv_font_montserrat_24; }
  static inline const lv_font_t* xxl()  { return &lv_font_montserrat_32; }
  static inline const lv_font_t* hero() { return &lv_font_montserrat_48; }
}

// ── Style helpers ─────────────────────────────────────────────────────────────
namespace Theme {

// Apply dark-surface card style to any lv_obj_t
inline void applyCard(lv_obj_t* obj, lv_color_t bg = TC::surface()) {
  lv_obj_set_style_bg_color(obj, bg, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, TC::border(), 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(obj, 10, 0);
  lv_obj_set_style_pad_all(obj, TS::lg, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

// Apply screen background
inline void applyScreenBg(lv_obj_t* scr) {
  lv_obj_set_style_bg_color(scr, TC::bg(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

// Create a styled label
inline lv_obj_t* label(lv_obj_t* parent, const char* txt,
                        const lv_font_t* font, lv_color_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  return l;
}

// Create a filled button with label
inline lv_obj_t* button(lv_obj_t* parent, const char* txt,
                         lv_color_t bg, lv_color_t fg,
                         int w, int h) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_bg_color(btn, bg, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn, 10, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_border_width(btn, 0, 0);

  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, txt);
  lv_obj_set_style_text_color(lbl, fg, 0);
  lv_obj_set_style_text_font(lbl, TF::lg(), 0);
  lv_obj_center(lbl);
  return btn;
}

// Header bar (full-width, 52 px)
inline lv_obj_t* headerBar(lv_obj_t* parent) {
  lv_obj_t* bar = lv_obj_create(parent);
  lv_obj_set_size(bar, LV_PCT(100), 52);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, TC::surface(), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_color(bar, TC::border(), LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_pad_hor(bar, TS::xl, 0);
  lv_obj_set_style_pad_ver(bar, 0, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  return bar;
}

// Indicator dot (colored circle + label)
inline lv_obj_t* statusDot(lv_obj_t* parent, const char* label_txt, bool ok) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_layout(row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, TS::sm, 0);

  lv_obj_t* dot = lv_obj_create(row);
  lv_obj_set_size(dot, 12, 12);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, ok ? TC::ready() : TC::muted(), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(dot, 0, 0);

  lv_obj_t* lbl = lv_label_create(row);
  lv_label_set_text(lbl, label_txt);
  lv_obj_set_style_text_color(lbl, ok ? TC::text() : TC::muted(), 0);
  lv_obj_set_style_text_font(lbl, TF::md(), 0);

  return row;
}

} // namespace Theme
