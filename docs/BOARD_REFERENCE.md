# Waveshare ESP32-S3-Touch-LCD-5 — Board Reference

## Hardware

| Item | Detail |
|------|--------|
| MCU | ESP32-S3, dual-core Xtensa LX7 @ 240 MHz |
| Flash | 16 MB (Quad/Octal SPI) |
| PSRAM | 8 MB OPI (Octal SPI, 80 MHz) |
| Display | 5" IPS RGB, 800 × 480, 16-bit parallel |
| Touch | GT911 capacitive, I²C |
| I/O expander | CH422G (backlight, LCD reset, touch reset) |
| RS485 | SP3485 half-duplex transceiver, auto-direction |
| USB | Native ESP32-S3 USB-JTAG/Serial (VID:PID = 303A:0009) |

## Useful Links

| Resource | URL |
|----------|-----|
| Product page | https://www.waveshare.com/esp32-s3-touch-lcd-5.htm |
| Wiki / Getting started | https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-5 |
| Schematic PDF | https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-5/ESP32-S3-Touch-LCD-5-Sch.pdf |
| ESP-IDF | https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/ |
| LVGL docs | https://docs.lvgl.io/8.4/ |
| esp-lvgl-port | https://components.espressif.com/components/espressif/esp_lvgl_port |
| GT911 driver | https://components.espressif.com/components/espressif/esp_lcd_touch_gt911 |

---

## Pin Map (verified against schematic)

### I²C — shared bus (GT911 touch + CH422G expander)

| Signal | GPIO |
|--------|------|
| SDA | GPIO8 |
| SCL | GPIO9 |

> **Note:** GPIO19/GPIO20 are the native USB D−/D+ pads. Do NOT use them for I²C or any other peripheral while USB is active.

### CH422G I/O Expander

The CH422G uses a non-standard multi-address I²C protocol:

| I²C Address | Purpose | Byte to write |
|-------------|---------|---------------|
| 0x24 | Set output mode | `0x01` |
| 0x38 | Drive outputs | `0x00` = all reset, `0x07` = all released (backlight ON) |

Output bit map at address 0x38:

| Bit | Signal |
|-----|--------|
| 0 | LCD_RST (active low reset) |
| 1 | CTP_RST / Touch reset |
| 2 | Backlight enable |

### GT911 Touch Controller

| Item | Value |
|------|-------|
| I²C address | 0x5D (INT low during reset) or 0x14 (INT high) |
| INT pin | GPIO4 |
| RST | via CH422G bit 1 |

### RGB LCD — 16-bit parallel

| Signal | GPIO |
|--------|------|
| PCLK | GPIO7 |
| VSYNC | GPIO3 |
| HSYNC | GPIO46 |
| DE | GPIO5 |
| B3 (D0) | GPIO14 |
| B4 (D1) | GPIO38 |
| B5 (D2) | GPIO18 |
| B6 (D3) | GPIO17 |
| B7 (D4) | GPIO10 |
| G2 (D5) | GPIO39 |
| G3 (D6) | GPIO0 |
| G4 (D7) | GPIO45 |
| G5 (D8) | GPIO48 |
| G6 (D9) | GPIO47 |
| G7 (D10) | GPIO21 |
| R3 (D11) | GPIO1 |
| R4 (D12) | GPIO2 |
| R5 (D13) | GPIO42 |
| R6 (D14) | GPIO41 |
| R7 (D15) | GPIO40 |

Data bus ordering: D[0:4] = B[4:0], D[5:10] = G[5:0], D[11:15] = R[4:0]
(RGB888 MSBs → RGB565, net names from schematic: B3..B7, G2..G7, R3..R7)

### RS485 — onboard SP3485 transceiver

| Signal | GPIO | Note |
|--------|------|------|
| UART TX | GPIO43 | MCU → RS485 transmitter |
| UART RX | GPIO44 | RS485 receiver → MCU |
| UART port | UART_NUM_1 | UART0 (GPIO43/44) is available for debug if RS485 is idle |
| Baud | 9600 (project default) | |
| Slave address | 1 (KC868-A6) | |

Auto-direction: the SP3485 DE/RE pins are wired to auto-switch on data activity — no GPIO control needed.

### USB

| Item | Value |
|------|-------|
| D− | GPIO19 (shared with USB-JTAG internal pad) |
| D+ | GPIO20 (shared with USB-JTAG internal pad) |
| VID:PID | 303A:0009 (ESP32-S3 USB-JTAG/Serial) |
| Windows driver | "USB Serial Device" — installed automatically |

---

## Working sdkconfig.defaults

```
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y

# PSRAM — 8 MB OPI
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384

# Flash — 16 MB
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"

# FreeRTOS — 1 ms tick
CONFIG_FREERTOS_HZ=1000

# USB CDC console (native USB on GPIO19/20)
CONFIG_ESP_CONSOLE_USB_CDC=y

# RGB LCD — IRAM-safe ISR off (lvgl_port vsync callback not IRAM_ATTR)
CONFIG_LCD_RGB_ISR_IRAM_SAFE=n

# Stack overflow detection
CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y

# LVGL fonts
CONFIG_LV_FONT_MONTSERRAT_14=y
CONFIG_LV_FONT_MONTSERRAT_16=y
CONFIG_LV_FONT_MONTSERRAT_20=y
CONFIG_LV_FONT_MONTSERRAT_24=y
CONFIG_LV_FONT_MONTSERRAT_32=y
CONFIG_LV_FONT_MONTSERRAT_48=y
CONFIG_LV_USE_FLEX=y
```

---

## LCD Timing Parameters (confirmed working)

```cpp
.pclk_hz           = 16'000'000   // 16 MHz
.h_res             = 800
.v_res             = 480
.hsync_pulse_width = 40
.hsync_back_porch  = 40
.hsync_front_porch = 48
.vsync_pulse_width = 23
.vsync_back_porch  = 32
.vsync_front_porch = 13
.flags.pclk_active_neg = true     // data latched on falling PCLK edge
```

**Critical:** `pclk_active_neg = true` — without this the image scrolls horizontally.

**Critical:** `bounce_buffer_size_px = 10 * 800` — without this, direct PSRAM reads cannot sustain 16 MHz pixel clock and the image scrolls. The bounce buffer copies rows into internal SRAM first.

### LVGL Port (esp_lvgl_port)

```cpp
// lvgl_port_display_cfg_t
.buffer_size   = 800 * 40        // 40-line partial render strip
.double_buffer = false
.flags.buff_spiram  = true
.flags.full_refresh = false

// lvgl_port_display_rgb_cfg_t
.flags.bb_mode       = 1         // use bounce-buffer callback
.flags.avoid_tearing = 0
```

---

## Known Pitfalls

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| Image scrolls left/right | PSRAM too slow for 16 MHz direct DMA | Enable `bounce_buffer_size_px = 10 * 800` |
| Image scrolls left/right | Wrong PCLK polarity | Set `pclk_active_neg = true` |
| Black screen after I²C init | I²C on GPIO19/20 kills USB (D−/D+) | Use GPIO8/9 for I²C |
| CH422G "no ack" | Bus stuck after crash | 3-retry loop at 100 kHz + 50 ms startup delay |
| GT911 I²C flood | `tp_io` handle leaked on failed init | Call `esp_lcd_panel_io_del(tp_io)` on failure |
| `lvgl_port_lock` crash | `trans_sem` NULL with `full_refresh=true, avoid_tearing=0` | Use partial render (full_refresh=false) or avoid_tearing=1 |
| IRAM assertion on boot | `CONFIG_LCD_RGB_ISR_IRAM_SAFE=y` with flash callbacks | Set `CONFIG_LCD_RGB_ISR_IRAM_SAFE=n` |

---

## Flash / Monitor Commands

```powershell
# Set up environment (run once per terminal session)
$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.5.4"
$env:IDF_TOOLS_PATH = "C:\Espressif"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\python_env\idf5.5_py3.11_env"
$env:PYTHONNOUSERSITE = "True"
$env:PATH = "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;" +
            "C:\Espressif\tools\idf-git\2.44.0\cmd;" +
            "C:\Espressif\tools\cmake\3.30.2\bin;" +
            "C:\Espressif\tools\ninja\1.12.1;" +
            "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;" +
            "C:\Espressif;" + $env:PATH

# Build
cd firmware\lpg_display
python $env:IDF_PATH\tools\idf.py build

# Flash (replace COMx with actual port)
cd build
python -m esptool --chip esp32s3 -p COMx -b 460800 `
    --before default_reset --after hard_reset `
    write_flash "@flash_args"
```

The board USB port alternates between COM8 and COM9 after replug — check Device Manager for the current assignment ("USB Serial Device").
