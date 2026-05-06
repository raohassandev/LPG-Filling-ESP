#include "Bsp.h"
#include "DisplayConfig.h"

#include "esp_log.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_io_expander_ch422g.h"
#include "esp_lvgl_port.h"
#include "driver/i2c.h"

static const char* TAG = "BSP";

static esp_lcd_panel_handle_t       s_panel  = nullptr;
static esp_lcd_touch_handle_t       s_touch  = nullptr;
static esp_io_expander_handle_t     s_expander = nullptr;

// ── I2C ───────────────────────────────────────────────────────────────────────
static esp_err_t initI2c() {
    i2c_config_t cfg = {
        .mode          = I2C_MODE_MASTER,
        .sda_io_num    = kI2cSda,
        .scl_io_num    = kI2cScl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master        = { .clk_speed = kI2cHz },
        .clk_flags     = 0,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(I2C_NUM_0, &cfg), TAG, "I2C param");
    ESP_RETURN_ON_ERROR(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0), TAG, "I2C install");
    ESP_LOGI(TAG, "I2C ready SDA=%d SCL=%d", kI2cSda, kI2cScl);
    return ESP_OK;
}

// ── CH422G I/O expander (controls backlight + LCD/touch reset) ────────────────
static esp_err_t initExpander() {
    ESP_RETURN_ON_ERROR(
        esp_io_expander_new_i2c_ch422g(I2C_NUM_0, kCh422gAddr, &s_expander),
        TAG, "CH422G init");

    // Set all output pins high:
    //   bit 0 = LCD reset (active-low on some boards — assert then deassert)
    //   bit 1 = Touch reset
    //   bit 2 = Backlight enable
    esp_io_expander_set_dir(s_expander, IO_EXPANDER_PIN_NUM_0 |
                                         IO_EXPANDER_PIN_NUM_1 |
                                         IO_EXPANDER_PIN_NUM_2, IO_EXPANDER_OUTPUT);
    // Reset pulse
    esp_io_expander_set_level(s_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    esp_io_expander_set_level(s_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 |
                                           IO_EXPANDER_PIN_NUM_2, 1);  // backlight ON
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "CH422G ready, backlight ON");
    return ESP_OK;
}

// ── RGB LCD panel ─────────────────────────────────────────────────────────────
static esp_err_t initLcd() {
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src         = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz             = (uint32_t)kLcdPclkHz,
            .h_res               = (uint32_t)kLcdHres,
            .v_res               = (uint32_t)kLcdVres,
            .hsync_back_porch    = (uint32_t)kLcdHbp,
            .hsync_front_porch   = (uint32_t)kLcdHfp,
            .hsync_pulse_width   = (uint32_t)kLcdHpw,
            .vsync_back_porch    = (uint32_t)kLcdVbp,
            .vsync_front_porch   = (uint32_t)kLcdVfp,
            .vsync_pulse_width   = (uint32_t)kLcdVpw,
            .flags = { .pclk_active_neg = 0 },
        },
        .data_width      = 16,
        .num_fbs         = 2,              // double-buffer in PSRAM
        .psram_trans_align = 64,
        .hsync_gpio_num  = kLcdHsync,
        .vsync_gpio_num  = kLcdVsync,
        .de_gpio_num     = kLcdDe,
        .pclk_gpio_num   = kLcdPclk,
        .disp_gpio_num   = GPIO_NUM_NC,
        .data_gpio_nums  = {
            kLcdData[0],  kLcdData[1],  kLcdData[2],  kLcdData[3],
            kLcdData[4],  kLcdData[5],  kLcdData[6],  kLcdData[7],
            kLcdData[8],  kLcdData[9],  kLcdData[10], kLcdData[11],
            kLcdData[12], kLcdData[13], kLcdData[14], kLcdData[15],
        },
        .flags = {
            .fb_in_psram    = 1,
            .double_fb      = 1,
            .refresh_on_demand = 0,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&cfg, &s_panel), TAG, "RGB panel create");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel),           TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel),            TAG, "panel init");
    ESP_LOGI(TAG, "RGB LCD %dx%d ready", kLcdHres, kLcdVres);
    return ESP_OK;
}

// ── GT911 touch ───────────────────────────────────────────────────────────────
static esp_err_t initTouch() {
    esp_lcd_panel_io_handle_t tp_io;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_cfg.dev_addr = kTouchAddr;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_NUM_0, &tp_io_cfg, &tp_io),
        TAG, "touch IO");

    esp_lcd_touch_config_t tp_cfg = {
        .x_max        = (uint16_t)kLcdHres,
        .y_max        = (uint16_t)kLcdVres,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels       = { .reset = 0, .interrupt = 0 },
        .flags        = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &s_touch),
                         TAG, "GT911 init");
    ESP_LOGI(TAG, "GT911 touch ready");
    return ESP_OK;
}

// ── LVGL port ─────────────────────────────────────────────────────────────────
static esp_err_t initLvgl() {
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port init");

    const lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle   = s_panel,
        .buffer_size    = (uint32_t)(kLcdHres * 40),
        .double_buffer  = true,
        .hres           = (uint32_t)kLcdHres,
        .vres           = (uint32_t)kLcdVres,
        .monochrome     = false,
        .rotation       = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .flags          = { .buff_dma = false, .buff_spiram = true, .swap_bytes = false },
    };
    lv_disp_t* disp = lvgl_port_add_disp(&disp_cfg);
    if (!disp) { ESP_LOGE(TAG, "lvgl_port_add_disp failed"); return ESP_FAIL; }

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp   = disp,
        .handle = s_touch,
    };
    lv_indev_t* indev = lvgl_port_add_touch(&touch_cfg);
    if (!indev) { ESP_LOGE(TAG, "lvgl_port_add_touch failed"); return ESP_FAIL; }

    ESP_LOGI(TAG, "LVGL port ready");
    return ESP_OK;
}

// ── Public API ────────────────────────────────────────────────────────────────
bool Bsp::init() {
    if (initI2c()      != ESP_OK) return false;
    if (initExpander() != ESP_OK) return false;
    if (initLcd()      != ESP_OK) return false;
    if (initTouch()    != ESP_OK) return false;
    if (initLvgl()     != ESP_OK) return false;
    ESP_LOGI(TAG, "BSP init complete");
    return true;
}

bool Bsp::lock(uint32_t timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void Bsp::unlock() {
    lvgl_port_unlock();
}
