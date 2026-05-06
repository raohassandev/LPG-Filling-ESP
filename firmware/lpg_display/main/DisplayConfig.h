#pragma once
#include "driver/gpio.h"
#include "driver/uart.h"

// ── Waveshare ESP32-S3-Touch-LCD-5 pin map ────────────────────────────────────
// Verify all pin numbers against the official schematic:
// https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-5  → Resources → Schematic PDF
// ─────────────────────────────────────────────────────────────────────────────

// RS485 — onboard SP3485 transceiver, auto-direction
static constexpr uart_port_t kRtuUart   = UART_NUM_1;
static constexpr gpio_num_t  kRtuTxPin  = GPIO_NUM_43;
static constexpr gpio_num_t  kRtuRxPin  = GPIO_NUM_44;
static constexpr int         kRtuBaud   = 9600;
static constexpr uint8_t     kRtuAddr   = 1;      // KC868-A6 slave address

// I2C — shared bus for GT911 touch + CH422G expander
static constexpr gpio_num_t  kI2cSda    = GPIO_NUM_19;
static constexpr gpio_num_t  kI2cScl    = GPIO_NUM_20;
static constexpr uint32_t    kI2cHz     = 400000;

// GT911 touch — address depends on INT pin state at reset:
//   0x5D when INT is LOW during reset (default if INT is driven)
//   0x14 when INT floats HIGH (common on Waveshare boards with no INT pull-down)
static constexpr uint8_t     kTouchAddr = 0x14;

// CH422G I/O expander (backlight, LCD reset, touch reset)
static constexpr uint8_t     kCh422gAddr = 0x24;

// LCD RGB interface — 16-bit parallel
static constexpr gpio_num_t  kLcdPclk   = GPIO_NUM_12;
static constexpr gpio_num_t  kLcdVsync  = GPIO_NUM_40;
static constexpr gpio_num_t  kLcdHsync  = GPIO_NUM_39;
static constexpr gpio_num_t  kLcdDe     = GPIO_NUM_41;
// Data: B[4:0], G[5:0], R[4:0]  (order matches esp_lcd_rgb_panel data_gpio_nums)
static constexpr gpio_num_t  kLcdData[16] = {
    GPIO_NUM_14, GPIO_NUM_21, GPIO_NUM_47, GPIO_NUM_48, GPIO_NUM_45,  // B[4:0]
    GPIO_NUM_4,  GPIO_NUM_16, GPIO_NUM_15, GPIO_NUM_7,  GPIO_NUM_6,  GPIO_NUM_5, // G[5:0]
    GPIO_NUM_1,  GPIO_NUM_46, GPIO_NUM_3,  GPIO_NUM_42, GPIO_NUM_2,  // R[4:0]
};

// LCD timing (800×480 @ ~16 MHz pixel clock)
static constexpr int  kLcdHres     = 800;
static constexpr int  kLcdVres     = 480;
static constexpr int  kLcdPclkHz   = 16000000;
static constexpr int  kLcdHbp      = 8;    // hsync back porch
static constexpr int  kLcdHfp      = 8;    // hsync front porch
static constexpr int  kLcdHpw      = 4;    // hsync pulse width
static constexpr int  kLcdVbp      = 16;   // vsync back porch
static constexpr int  kLcdVfp      = 16;   // vsync front porch
static constexpr int  kLcdVpw      = 4;    // vsync pulse width

// Admin PIN
static constexpr uint32_t kAdminPin = 1234;
