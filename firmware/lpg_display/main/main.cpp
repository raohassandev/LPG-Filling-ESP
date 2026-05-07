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

static void showDisplaySelfTest() {
    if (!Bsp::lock()) return;

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, TC::bg(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    static const lv_color_t colors[] = {
        lv_color_hex(0xef4444),
        lv_color_hex(0xf59e0b),
        lv_color_hex(0x10b981),
        lv_color_hex(0x3b82f6),
        lv_color_hex(0x8b5cf6),
    };
    for (int i = 0; i < 5; i++) {
        lv_obj_t* bar = lv_obj_create(scr);
        lv_obj_set_size(bar, 160, 480);
        lv_obj_set_pos(bar, i * 160, 0);
        lv_obj_set_style_bg_color(bar, colors[i], 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_obj_t* panel = lv_obj_create(scr);
    lv_obj_set_size(panel, 560, 180);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, TC::surface(), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_90, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(panel);
    lv_label_set_text(title, "DISPLAY SELF TEST");
    lv_obj_set_style_text_font(title, TF::xxl(), 0);
    lv_obj_set_style_text_color(title, TC::white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 26);

    lv_obj_t* sub = lv_label_create(panel);
    lv_label_set_text(sub, "LCD, RGB bus, backlight and LVGL are running");
    lv_obj_set_style_text_font(sub, TF::lg(), 0);
    lv_obj_set_style_text_color(sub, TC::text(), 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 18);

    lv_obj_t* foot = lv_label_create(panel);
    lv_label_set_text(foot, "Next: LPG controller connection screen");
    lv_obj_set_style_text_font(foot, TF::md(), 0);
    lv_obj_set_style_text_color(foot, TC::textSub(), 0);
    lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, -22);

    lv_scr_load(scr);
    Bsp::unlock();
}

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
    // Show self-test colour bars for 2.5 s, then load dashboard.
    showDisplaySelfTest();
    vTaskDelay(pdMS_TO_TICKS(2500));

    screenManager.begin(modbusClient);

    for (;;) {
        const ControllerSnapshot& snap = modbusClient.snapshot();
        screenManager.update(snap, modbusClient);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

extern "C" void app_main() {
    ESP_LOGI(TAG, "LPG Display booting");

    if (!Bsp::init()) {
        ESP_LOGE(TAG, "BSP init failed — halting");
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    xTaskCreatePinnedToCore(modbusTask, "modbus",  4096, nullptr, 5, nullptr, 0);
    xTaskCreatePinnedToCore(uiTask,     "ui",     32768, nullptr, 4, nullptr, 1);

    ESP_LOGI(TAG, "Tasks started");
    vTaskDelete(nullptr);  // app_main can exit
}
