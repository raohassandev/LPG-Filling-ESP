#pragma once
#include <lvgl.h>

// ── Board Support Package ─────────────────────────────────────────────────────
// Implement Bsp.cpp once you know your exact board variant.
// Everything else in lpg_display is board-agnostic LVGL.
//
// Required: lv_conf.h in Arduino/libraries/ with:
//   LV_COLOR_DEPTH 16
//   LV_HOR_RES_MAX 800
//   LV_VER_RES_MAX 480
//   LV_FONT_MONTSERRAT_12/14/16/20/24/32/48  1
//   LV_USE_FLEX   1
// ─────────────────────────────────────────────────────────────────────────────

namespace Bsp {
  // Initialize display driver, touch driver, and call lv_init().
  // Returns false if init fails (e.g. touch not detected).
  bool init();

  // Call from loop() — feeds LVGL tick and touch events.
  void tick();
}
