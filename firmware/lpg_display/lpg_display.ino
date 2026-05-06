#include <Arduino.h>
#include <lvgl.h>
#include "DisplayConfig.h"
#include "Bsp.h"
#include "ModbusClient.h"
#include "ScreenManager.h"

// ── Global instances (extern'd by screen files) ───────────────────────────────
ModbusClient  modbusClient;
ScreenManager screenManager;

// LVGL 1 ms tick (called from timer ISR or FreeRTOS task)
static void lvglTickTask(void*) {
  for (;;) {
    lv_tick_inc(1);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[DISPLAY] LPG Display booting");

  if (!Bsp::init()) {
    Serial.println("[DISPLAY] BSP init failed — halting");
    while (true) delay(1000);
  }

  // LVGL tick task on core 0 (app runs on core 1)
  xTaskCreatePinnedToCore(lvglTickTask, "lvgl_tick", 2048, nullptr, 5, nullptr, 0);

  modbusClient.begin();

  // Build and show dashboard as first screen
  screenManager.begin();
  // Dashboard will be built on first update() once a valid snapshot arrives.
  // Show a splash label while waiting.
  lv_obj_t* splash = lv_obj_create(lv_scr_act());
  lv_obj_set_size(splash, 800, 480);
  lv_obj_set_style_bg_color(splash, lv_color_hex(0x0f1117), 0);
  lv_obj_set_style_bg_opa(splash, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(splash, 0, 0);
  lv_obj_center(splash);
  lv_obj_t* splashLbl = lv_label_create(splash);
  lv_label_set_text(splashLbl, "LPG FILLING STATION\nConnecting...");
  lv_obj_set_style_text_font(splashLbl, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(splashLbl, lv_color_hex(0xf0f2f5), 0);
  lv_obj_set_style_text_align(splashLbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(splashLbl);

  Serial.println("[DISPLAY] Setup complete");
}

void loop() {
  modbusClient.poll();
  const ControllerSnapshot& snap = modbusClient.snapshot();
  screenManager.update(snap, modbusClient);
  Bsp::tick();
  // No delay — LVGL timer_handler manages its own cadence
}
