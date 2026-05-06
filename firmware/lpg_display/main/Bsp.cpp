#include "Bsp.h"
#include "DisplayConfig.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lvgl_port.h"
#include "driver/i2c_master.h"

static const char* TAG = "BSP";

static esp_lcd_panel_handle_t  s_panel   = nullptr;
static esp_lcd_touch_handle_t  s_touch   = nullptr;
static i2c_master_bus_handle_t s_i2c_bus = nullptr;

// ── I2C (new master API, required by esp_lcd_touch v1.2) ─────────────────────
static esp_err_t initI2c() {
    i2c_master_bus_config_t cfg = {
        .i2c_port          = I2C_NUM_0,
        .sda_io_num        = kI2cSda,
        .scl_io_num        = kI2cScl,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = true },
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&cfg, &s_i2c_bus), TAG, "I2C master");
    ESP_LOGI(TAG, "I2C ready SDA=%d SCL=%d", kI2cSda, kI2cScl);
    return ESP_OK;
}

// ── CH422G I/O expander — direct I2C (multi-address protocol) ─────────────────
// CH422G responds to two I2C addresses:
//   0x24 — write config byte (bit 0 = enable output mode)
//   0x38 — write output levels (bit 2 = backlight, bit 1 = touch RST, bit 0 = LCD RST)
static esp_err_t ch422g_write(uint8_t dev_addr, uint8_t data) {
    i2c_master_dev_handle_t dev;
    i2c_device_config_t dcfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = dev_addr,
        .scl_speed_hz    = 400000,
    };
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(s_i2c_bus, &dcfg, &dev), TAG, "CH422G add dev");
    esp_err_t ret = i2c_master_transmit(dev, &data, 1, pdMS_TO_TICKS(50));
    i2c_master_bus_rm_device(dev);
    return ret;
}

static esp_err_t initExpander() {
    ESP_RETURN_ON_ERROR(ch422g_write(0x24, 0x01), TAG, "CH422G set output mode");
    ESP_RETURN_ON_ERROR(ch422g_write(0x38, 0x00), TAG, "CH422G reset assert");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(ch422g_write(0x38, 0x07), TAG, "CH422G reset release");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "CH422G ready, backlight ON");
    return ESP_OK;
}

// ── RGB LCD panel ─────────────────────────────────────────────────────────────
static esp_err_t initLcd() {
    // Field order must match esp_lcd_rgb_timing_t and esp_lcd_rgb_panel_config_t declarations
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src  = LCD_CLK_SRC_DEFAULT,
        .timings  = {
            .pclk_hz           = (uint32_t)kLcdPclkHz,
            .h_res             = (uint32_t)kLcdHres,
            .v_res             = (uint32_t)kLcdVres,
            .hsync_pulse_width = (uint32_t)kLcdHpw,   // must come before back/front porch
            .hsync_back_porch  = (uint32_t)kLcdHbp,
            .hsync_front_porch = (uint32_t)kLcdHfp,
            .vsync_pulse_width = (uint32_t)kLcdVpw,   // must come before back/front porch
            .vsync_back_porch  = (uint32_t)kLcdVbp,
            .vsync_front_porch = (uint32_t)kLcdVfp,
            .flags = { .pclk_active_neg = 0 },
        },
        .data_width        = 16,
        .num_fbs           = 2,
        .psram_trans_align = 64,
        .hsync_gpio_num    = kLcdHsync,
        .vsync_gpio_num    = kLcdVsync,
        .de_gpio_num       = kLcdDe,
        .pclk_gpio_num     = kLcdPclk,
        .disp_gpio_num     = GPIO_NUM_NC,
        .data_gpio_nums    = {
            kLcdData[0],  kLcdData[1],  kLcdData[2],  kLcdData[3],
            kLcdData[4],  kLcdData[5],  kLcdData[6],  kLcdData[7],
            kLcdData[8],  kLcdData[9],  kLcdData[10], kLcdData[11],
            kLcdData[12], kLcdData[13], kLcdData[14], kLcdData[15],
        },
        .flags = {
            .refresh_on_demand = 0,  // must be first in flags struct
            .fb_in_psram       = 1,
            .double_fb         = 1,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&cfg, &s_panel), TAG, "RGB panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel),           TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel),            TAG, "panel init");
    ESP_LOGI(TAG, "RGB LCD %dx%d ready", kLcdHres, kLcdVres);
    return ESP_OK;
}

// ── GT911 touch ───────────────────────────────────────────────────────────────
static esp_err_t initTouch() {
    // Manually init struct in correct field-declaration order (C++ requires this)
    // Struct order: dev_addr, on_color_trans_done, user_ctx, control_phase_bytes,
    //               dc_bit_offset, lcd_cmd_bits, lcd_param_bits, flags, scl_speed_hz
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = {
        .dev_addr             = kTouchAddr,
        .on_color_trans_done  = nullptr,
        .user_ctx             = nullptr,
        .control_phase_bytes  = 1,
        .dc_bit_offset        = 0,
        .lcd_cmd_bits         = 16,
        .lcd_param_bits       = 8,
        .flags                = { .disable_control_phase = 1 },
        .scl_speed_hz         = 100000,
    };
    esp_lcd_panel_io_handle_t tp_io;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_cfg, &tp_io),
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
        .io_handle     = nullptr,
        .panel_handle  = s_panel,
        .buffer_size   = (uint32_t)(kLcdHres * 40),
        .double_buffer = true,
        .hres          = (uint32_t)kLcdHres,
        .vres          = (uint32_t)kLcdVres,
        .monochrome    = false,
        .rotation      = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .flags         = {
            .buff_dma     = false,
            .buff_spiram  = true,
            .full_refresh = true,
        },
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = { .bb_mode = 0, .avoid_tearing = 0 },
    };
    lv_display_t* disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);
    if (!disp) { ESP_LOGE(TAG, "lvgl_port_add_disp_rgb failed"); return ESP_FAIL; }

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
