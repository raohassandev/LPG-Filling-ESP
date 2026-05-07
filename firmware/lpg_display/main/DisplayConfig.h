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
// GPIO8/9 per schematic (GPIO19/20 are USB D-/D+ — reserved for USB CDC console)
static constexpr gpio_num_t  kI2cSda    = GPIO_NUM_8;
static constexpr gpio_num_t  kI2cScl    = GPIO_NUM_9;
static constexpr uint32_t    kI2cHz     = 400000;

// GT911 touch — address depends on INT pin state at reset:
//   0x5D when INT is LOW during reset (default if INT is driven)
//   0x14 when INT floats HIGH (common on Waveshare boards with no INT pull-down)
// Scan showed 0x24 (CH422G) but not 0x14 or 0x5D — try 0x5D
static constexpr uint8_t     kTouchAddr = 0x5D;

// CH422G I/O expander (backlight, LCD reset, touch reset)
static constexpr uint8_t     kCh422gAddr = 0x24;

// LCD RGB interface — 16-bit parallel
// Pin assignments verified from Waveshare ESP32-S3-Touch-LCD-5 schematic PDF
static constexpr gpio_num_t  kLcdPclk   = GPIO_NUM_7;
static constexpr gpio_num_t  kLcdVsync  = GPIO_NUM_3;
static constexpr gpio_num_t  kLcdHsync  = GPIO_NUM_46;
static constexpr gpio_num_t  kLcdDe     = GPIO_NUM_5;
// Data: D[0..4]=B[4:0], D[5..10]=G[5:0], D[11..15]=R[4:0]
// Net names from schematic: B3..B7, G2..G7, R3..R7 (RGB888 MSBs → RGB565)
static constexpr gpio_num_t  kLcdData[16] = {
    GPIO_NUM_14, GPIO_NUM_38, GPIO_NUM_18, GPIO_NUM_17, GPIO_NUM_10,  // B[4:0]: B3..B7
    GPIO_NUM_39, GPIO_NUM_0,  GPIO_NUM_45, GPIO_NUM_48, GPIO_NUM_47, GPIO_NUM_21, // G[5:0]: G2..G7
    GPIO_NUM_1,  GPIO_NUM_2,  GPIO_NUM_42, GPIO_NUM_41, GPIO_NUM_40,  // R[4:0]: R3..R7
};

// LCD timing (800×480 @ 16 MHz pixel clock)
// Porch values for Waveshare ESP32-S3-Touch-LCD-5 (5" IPS RGB panel)
static constexpr int  kLcdHres     = 800;
static constexpr int  kLcdVres     = 480;
static constexpr int  kLcdPclkHz   = 16000000;
static constexpr int  kLcdHbp      = 40;   // hsync back porch
static constexpr int  kLcdHfp      = 48;   // hsync front porch
static constexpr int  kLcdHpw      = 40;   // hsync pulse width
static constexpr int  kLcdVbp      = 32;   // vsync back porch
static constexpr int  kLcdVfp      = 13;   // vsync front porch
static constexpr int  kLcdVpw      = 23;   // vsync pulse width

// Admin PIN
static constexpr uint32_t kAdminPin = 1234;
