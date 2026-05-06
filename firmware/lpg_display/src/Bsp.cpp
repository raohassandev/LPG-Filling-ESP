#include "Bsp.h"
#include <Arduino.h>

// ── TODO: Replace this file with your board's BSP ────────────────────────────
//
// Common options for ESP32-S3 5" display boards:
//
// 1. Sunton / Guition boards → use "ESP32_Display_Panel" library:
//    https://github.com/esp-arduino-libs/ESP32_Display_Panel
//    #include <ESP_Panel_Library.h>
//    ESP_Panel panel; panel.init(); panel.begin();
//
// 2. Elecrow / Waveshare boards → usually provide their own BSP header.
//
// 3. Custom wiring → wire SPI/RGB display driver (ST7796, ILI9488, etc.)
//    and I2C touch (FT5x06, GT911) manually using TFT_eSPI + lvgl.
//
// The LVGL tick (lv_tick_inc) must be called every 1 ms.
// The display flush and touch read callbacks must be registered.
// ─────────────────────────────────────────────────────────────────────────────

static lv_disp_draw_buf_t drawBuf;
static lv_color_t         buf1[800 * 20];   // ~32 KB — adjust if tight on RAM
static lv_disp_drv_t      dispDrv;
static lv_indev_drv_t     indevDrv;

// Called by LVGL when it has pixels to push to the screen
static void displayFlush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  // TODO: write pixels at (area->x1,area->y1)..(area->x2,area->y2) to display
  // e.g.: tft.pushImage(area->x1, area->y1, w, h, (uint16_t*)color_p);
  lv_disp_flush_ready(drv);
}

// Called by LVGL to read touch
static void touchRead(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  // TODO: read touch controller (FT6336, GT911, etc.)
  // If touched:
  //   data->point.x = tx; data->point.y = ty;
  //   data->state   = LV_INDEV_STATE_PR;
  // else:
  //   data->state   = LV_INDEV_STATE_REL;
  data->state = LV_INDEV_STATE_REL;
}

bool Bsp::init() {
  lv_init();

  lv_disp_draw_buf_init(&drawBuf, buf1, nullptr, 800 * 20);

  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res    = 800;
  dispDrv.ver_res    = 480;
  dispDrv.flush_cb   = displayFlush;
  dispDrv.draw_buf   = &drawBuf;
  lv_disp_drv_register(&dispDrv);

  lv_indev_drv_init(&indevDrv);
  indevDrv.type     = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb  = touchRead;
  lv_indev_drv_register(&indevDrv);

  Serial.println("[BSP] LVGL initialized (display flush stub — wire your driver)");
  return true;
}

void Bsp::tick() {
  lv_timer_handler();
}
