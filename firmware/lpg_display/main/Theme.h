#pragma once
#include "lvgl.h"

namespace TC {
    static inline lv_color_t bg()       { return lv_color_hex(0x111315); }
    static inline lv_color_t surface()  { return lv_color_hex(0x1b2024); }
    static inline lv_color_t surface2() { return lv_color_hex(0x252b30); }
    static inline lv_color_t border()   { return lv_color_hex(0x3a424a); }
    static inline lv_color_t text()     { return lv_color_hex(0xf4f6f8); }
    static inline lv_color_t textSub()  { return lv_color_hex(0xb8c0c8); }
    static inline lv_color_t muted()    { return lv_color_hex(0x717b85); }
    static inline lv_color_t ready()    { return lv_color_hex(0x22c55e); }
    static inline lv_color_t warning()  { return lv_color_hex(0xf59e0b); }
    static inline lv_color_t danger()   { return lv_color_hex(0xdc2626); }
    static inline lv_color_t active()   { return lv_color_hex(0x0f9f8f); }
    static inline lv_color_t settling() { return lv_color_hex(0x7c3aed); }
    static inline lv_color_t complete() { return lv_color_hex(0x14b8a6); }
    static inline lv_color_t white()    { return lv_color_hex(0xffffff); }
    static inline lv_color_t black()    { return lv_color_hex(0x000000); }
}

namespace TS { enum { xs=4, sm=8, md=12, lg=16, xl=24, xxl=32 }; }

namespace TF {
    static inline const lv_font_t* sm()   { return &lv_font_montserrat_14; }
    static inline const lv_font_t* md()   { return &lv_font_montserrat_16; }
    static inline const lv_font_t* lg()   { return &lv_font_montserrat_20; }
    static inline const lv_font_t* xl()   { return &lv_font_montserrat_24; }
    static inline const lv_font_t* xxl()  { return &lv_font_montserrat_32; }
    static inline const lv_font_t* hero() { return &lv_font_montserrat_48; }
}

namespace Theme {

inline void applyCard(lv_obj_t* obj, lv_color_t bg = TC::surface()) {
    lv_obj_set_style_bg_color(obj, bg, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, TC::border(), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 10, 0);
    lv_obj_set_style_pad_all(obj, TS::lg, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

inline void applyScreenBg(lv_obj_t* scr) {
    lv_obj_set_style_bg_color(scr, TC::bg(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

inline lv_obj_t* headerBar(lv_obj_t* parent) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_PCT(100), 52);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, TC::surface(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, TS::xl, 0);
    lv_obj_set_style_pad_ver(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    return bar;
}

inline lv_obj_t* label(lv_obj_t* parent, const char* txt,
                        const lv_font_t* font, lv_color_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

inline lv_obj_t* button(lv_obj_t* parent, const char* txt,
                         lv_color_t bg, lv_color_t fg, int w, int h) {
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

inline lv_obj_t* statusDot(lv_obj_t* parent, const char* txt, bool ok) {
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
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_color(lbl, ok ? TC::text() : TC::muted(), 0);
    lv_obj_set_style_text_font(lbl, TF::md(), 0);
    return row;
}

} // namespace Theme
