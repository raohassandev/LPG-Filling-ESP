#include "Bsp.h"
#include <Arduino.h>

// ── Waveshare ESP32-S3-Touch-LCD-5 BSP ───────────────────────────────────────
// Library required: "ESP32_Display_Panel" by Espressif (Arduino Library Manager)
// Also install:     "ESP32_IO_Expander"   by Espressif
//
// Board manager: "esp32" by Espressif 3.x
// Board target:  "ESP32S3 Dev Module"
//   Partition:    16M Flash (3MB APP/9.9MB FATFS)  or  Huge APP
//   PSRAM:        OPI PSRAM
//   Flash Mode:   QIO 80MHz
//
// In Arduino IDE: Tools → USB CDC On Boot → Enabled  (keeps Serial on USB)
// ─────────────────────────────────────────────────────────────────────────────

#include <ESP_Panel_Library.h>
#include <lvgl.h>

static ESP_Panel* panel = nullptr;

// LVGL draw buffer — two half-screen buffers for smooth rendering
static lv_disp_draw_buf_t drawBuf;
static lv_color_t buf1[800 * 40];   // ~64 KB — fits in PSRAM
static lv_color_t buf2[800 * 40];

static lv_disp_drv_t  dispDrv;
static lv_indev_drv_t indevDrv;

// ── Display flush callback ────────────────────────────────────────────────────
static void displayFlush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  panel->getLcd()->drawBitmap(area->x1, area->y1,
                               area->x2 - area->x1 + 1,
                               area->y2 - area->y1 + 1,
                               (const uint8_t*)color_p);
  lv_disp_flush_ready(drv);
}

// ── Touch read callback ───────────────────────────────────────────────────────
static void touchRead(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  ESP_PanelTouch* touch = panel->getTouch();
  if (!touch) { data->state = LV_INDEV_STATE_REL; return; }

  ESP_PanelTouchPoint point[1];
  int num = touch->readPoints(point, 1, -1);
  if (num > 0) {
    data->point.x = point[0].x;
    data->point.y = point[0].y;
    data->state   = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ── Bsp::init ─────────────────────────────────────────────────────────────────
bool Bsp::init() {
  // Init display panel (handles LCD driver, backlight, touch, I/O expander)
  panel = new ESP_Panel();
  if (!panel->init()) {
    Serial.println("[BSP] Panel init failed");
    return false;
  }
  if (!panel->begin()) {
    Serial.println("[BSP] Panel begin failed");
    return false;
  }

  // Reset touch
  if (panel->getTouch()) panel->getTouch()->swapXY(false);

  lv_init();

  // Use PSRAM for draw buffers (board has 8 MB OPI PSRAM)
  lv_disp_draw_buf_init(&drawBuf, buf1, buf2, 800 * 40);

  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res  = 800;
  dispDrv.ver_res  = 480;
  dispDrv.flush_cb = displayFlush;
  dispDrv.draw_buf = &drawBuf;
  dispDrv.full_refresh = 0;
  lv_disp_drv_register(&dispDrv);

  lv_indev_drv_init(&indevDrv);
  indevDrv.type    = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = touchRead;
  lv_indev_drv_register(&indevDrv);

  Serial.println("[BSP] Waveshare ESP32-S3-Touch-LCD-5 initialized");
  return true;
}

// ── Bsp::tick ─────────────────────────────────────────────────────────────────
void Bsp::tick() {
  lv_timer_handler();
}
