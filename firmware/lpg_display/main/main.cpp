#include "Bsp.h"
#include "ModbusClient.h"
#include "ScreenManager.h"
#include "Theme.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "MAIN";

ModbusClient  modbusClient;
ScreenManager screenManager;

// ── Modbus task (core 0, 4 KB stack) ──────────────────────────────────────────
static void modbusTask(void*) {
    modbusClient.begin();
    for (;;) {
        modbusClient.poll();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── UI task (core 1, 8 KB stack) ──────────────────────────────────────────────
static void uiTask(void*) {
    screenManager.begin();

    // Splash screen while waiting for first valid snapshot
    if (Bsp::lock()) {
        lv_obj_t* scr = lv_obj_create(nullptr);
        Theme::applyScreenBg(scr);
        lv_obj_t* lbl = lv_label_create(scr);
        lv_label_set_text(lbl, "LPG FILLING STATION\nConnecting to controller...");
        lv_obj_set_style_text_font(lbl, TF::xl(), 0);
        lv_obj_set_style_text_color(lbl, TC::text(), 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl);
        lv_scr_load(scr);
        Bsp::unlock();
    }

    for (;;) {
        const ControllerSnapshot& snap = modbusClient.snapshot();
        screenManager.update(snap, modbusClient);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

extern "C" void app_main() {
    ESP_LOGI(TAG, "LPG Display booting");

    if (!Bsp::init()) {
        ESP_LOGE(TAG, "BSP init failed — halting");
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    xTaskCreatePinnedToCore(modbusTask, "modbus", 4096, nullptr, 5, nullptr, 0);
    xTaskCreatePinnedToCore(uiTask,     "ui",     8192, nullptr, 4, nullptr, 1);

    ESP_LOGI(TAG, "Tasks started");
    vTaskDelete(nullptr);  // app_main can exit
}
