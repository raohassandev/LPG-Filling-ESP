# LPG Filling Station — Developer Guide

**Document version:** 1.0  
**Last updated:** 2026-05-07

---

## Table of Contents

1. [Repository Layout](#1-repository-layout)
2. [Hardware Overview](#2-hardware-overview)
3. [Controller Firmware (KC868-A6)](#3-controller-firmware-kc868-a6)
4. [Display Firmware (Waveshare ESP32-S3)](#4-display-firmware-waveshare-esp32-s3)
5. [RS485 / Modbus RTU Link](#5-rs485--modbus-rtu-link)
6. [Modbus Register Map](#6-modbus-register-map)
7. [Load Cell Calibration](#7-load-cell-calibration)
8. [Building & Flashing](#8-building--flashing)
9. [sdkconfig Notes](#9-sdkconfig-notes)
10. [Display UI Architecture](#10-display-ui-architecture)
11. [Adding a New Screen](#11-adding-a-new-screen)
12. [WiFi Manager](#12-wifi-manager)
13. [Known Issues & Gotchas](#13-known-issues--gotchas)

---

## 1. Repository Layout

```
firmware/
  lpg_controller/        Arduino sketch — KC868-A6 controller
    include/             Header files
    src/                 Source files
    lpg_controller.ino   Entry point (setup/loop)
  lpg_display/           ESP-IDF project — Waveshare touchscreen display
    main/                Application source
      screens/           One .h/.cpp per screen (Dashboard, Wifi, Pin, Settings…)
      ModbusClient.h/cpp RS485 Modbus RTU master
      WifiManager.h/cpp  WiFi STA+AP with NVS persistence
      ScreenManager.h/cpp Screen routing / auto-transition
      Theme.h            LVGL colour palette, font helpers, widget helpers
      DisplayConfig.h    All GPIO pin assignments for the display board
    sdkconfig.defaults   Baseline Kconfig settings (always edit this, not sdkconfig)
    CMakeLists.txt
docs/
  user/                  End-user documentation
  DEVELOPER_GUIDE.md     This file
scripts/
  build_firmware.ps1     Build controller (arduino-cli)
  upload_firmware.ps1    Flash controller
```

---

## 2. Hardware Overview

### Controller — KC868-A6

| Item | Value |
|------|-------|
| MCU | ESP32 dual-core 240 MHz |
| Flash | 4 MB |
| RS485 TX | GPIO27 (UART2) |
| RS485 RX | GPIO14 (UART2) |
| HX711 DOUT | GPIO32 (IO-1 terminal) |
| HX711 SCK | GPIO33 (IO-2 terminal) |
| I²C SDA | GPIO4 |
| I²C SCL | GPIO15 |
| RTC (DS3231) | I²C 0x68 |
| Relay expander (PCF8574) | I²C 0x24, 6 relays, active-low |
| Input expander (PCF8574) | I²C 0x22, 6 digital inputs |

### Display — Waveshare ESP32-S3-Touch-LCD-5

| Item | Value |
|------|-------|
| MCU | ESP32-S3 dual-core 240 MHz |
| Flash | 16 MB |
| PSRAM | 8 MB OPI |
| LCD | 800×480 RGB565, 16-bit parallel |
| Touch | GT911 I²C (addr 0x5D) |
| RS485 TX | GPIO43 |
| RS485 RX | GPIO44 |
| I²C SDA | GPIO8 |
| I²C SCL | GPIO9 |
| I/O expander (CH422G) | I²C 0x24 (backlight, reset) |

> **Pin naming trap:** The board reference is the source of truth for the display UART mapping:
> GPIO43 = ESP32-S3 TX to the RS485 transmitter, GPIO44 = ESP32-S3 RX from the RS485 receiver.

---

## 3. Controller Firmware (KC868-A6)

### 3.1 Framework

Arduino / ESP32 Arduino Core. Build tool: `arduino-cli`.

### 3.2 Key Classes

| Class | File | Responsibility |
|-------|------|----------------|
| `WeightService` | `src/WeightService.cpp` | HX711 bit-bang driver, stability detection, 2-point calibration |
| `FillController` | `src/FillController.cpp` | Fill state machine (Idle→Ready→…→Complete) |
| `StatusStore` | `src/StatusStore.cpp` | Thread-safe snapshot of all process variables |
| `ModbusRtuService` | `src/ModbusRtuService.cpp` | Modbus RTU server on UART2 (RS485) |
| `ModbusTcpService` | `src/ModbusTcpService.cpp` | Modbus TCP server on port 502 |
| `ModbusRegisterMap` | `src/ModbusRegisterMap.cpp` | Encode/decode register values — single source of truth |
| `SettingsStore` | `src/SettingsStore.cpp` | NVS-persisted settings (rate, WiFi, RTU params) |
| `TransactionLog` | `src/TransactionLog.cpp` | SPIFFS transaction log with today/week/month/year stats |
| `WebPortal` | `src/WebPortal.cpp` | HTTP REST API + serve web app |
| `RtcService` | `src/RtcService.cpp` | DS3231 RTC read/write |

### 3.3 Loop Structure

```cpp
loop() {
  weightService.poll();           // read HX711 if data ready
  statusStore.setLiveWeight(...); // update shared snapshot
  fillController.loop();          // advance state machine
  modbusRtu.handleClient();       // serve any incoming RTU frame
  modbusTcp.handleClient();       // serve any incoming TCP frame
  webPortal.loop();               // handle HTTP client
  // ... OLED, heartbeat, serial console
}
```

---

## 4. Display Firmware (Waveshare ESP32-S3)

### 4.1 Framework

ESP-IDF v5.5.4 with esp_lvgl_port v2.7.x (LVGL 8.4.x).

### 4.2 Task Layout

| Task | Core | Stack | Priority | Role |
|------|------|-------|----------|------|
| `modbusTask` | 0 | 4 KB | 5 | Poll Modbus RTU every 200 ms (fast) / 2 s (RTC) / 5 s (stats) |
| `wifiTask` | 0 | 6 KB | 4 | WiFi event loop and reconnect logic |
| `uiTask` | 1 | 32 KB | 5 | LVGL tick + screen updates at 100 ms |

### 4.3 LVGL Threading

All LVGL calls must be made under `Bsp::lock()` / `Bsp::unlock()`. These wrap the LVGL port mutex. Violating this causes hard faults.

```cpp
if (Bsp::lock()) {
    lv_label_set_text(lbl, "Hello");
    Bsp::unlock();
}
```

### 4.4 Screen Routing

`ScreenManager` holds the current and pending screen. On each update cycle:
1. Check if controller state demands a new screen (e.g. Fast Fill → FillProgress).
2. Apply any `navigateTo()` call made from a button callback.
3. Call the current screen's `update()`.

Auto-transition rules (when not on Pin/Settings/Wifi):

| Controller State | Screen shown |
|-----------------|--------------|
| Idle, Ready, Maintenance | Dashboard |
| Validating, Fast, Slow, Settling | FillProgress |
| Complete | FillComplete |
| Fault, Aborted | Fault |

---

## 5. RS485 / Modbus RTU Link

### 5.1 Physical Wiring

```
Display board                Controller board
──────────────────           ──────────────────
GPIO43 (TX) ──A──────────────A── GPIO27 (TX)
GPIO44 (RX) ──B──────────────B── GPIO14 (RX)
GND ─────────────────────────GND
```

Both boards use auto-direction RS485 transceivers — no DE/RE pin needed.

### 5.2 Protocol Parameters

| Parameter | Value |
|-----------|-------|
| Baud | 9600 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Slave address | 1 |

### 5.3 Polling Intervals

| Poll group | Interval | Registers |
|------------|----------|-----------|
| Fast | 200 ms | 0x0000–0x0017 (weights, state, flags) |
| Slow (RTC) | 2 s | 0x0020–0x0025 (hour, minute, second) |
| Stats | 5 s | 0x0030–0x0035 (today fills/kg/amount) |

### 5.4 Self-Echo Flush (critical)

RS485 half-duplex causes the TX bytes to echo back into RX. The display firmware
flushes the UART after TX completes, **before** reading the response:

```cpp
uart_write_bytes(kRtuUart, req, reqLen);
uart_wait_tx_done(kRtuUart, pdMS_TO_TICKS(50));
uart_flush_input(kRtuUart);   // ← must come after wait_tx_done, not before
int rx = uart_read_bytes(...);
```

Omitting the flush causes every read to return the echoed request instead of the response.

---

## 6. Modbus Register Map

Full register map is defined in `firmware/lpg_controller/src/ModbusRegisterMap.cpp`.  
See also [docs/user/MODBUS_PROTOCOL.md](user/MODBUS_PROTOCOL.md).

### Fast registers (FC03, start=0x0000, count=24)

| Offset | Reg pair | Type | Description |
|--------|----------|------|-------------|
| 0x00–0x01 | HR0–HR1 | float32 (hi,lo) | Live weight (kg) |
| 0x02–0x03 | HR2–HR3 | float32 | Tare weight (kg) |
| 0x04–0x05 | HR4–HR5 | float32 | Net weight (kg) |
| 0x06–0x07 | HR6–HR7 | float32 | Target weight (kg) |
| 0x08–0x09 | HR8–HR9 | float32 | Rate per kg (PKR) |
| 0x0A–0x0B | HR10–HR11 | float32 | Target amount (PKR) |
| 0x0C–0x0D | HR12–HR13 | float32 | Current amount (PKR) |
| 0x0E | HR14 | uint16 | Fill state (0=Idle…9=Maintenance) |
| 0x0F | HR15 | uint16 | E-Stop OK (1=safe) |
| 0x10 | HR16 | uint16 | Cylinder present (1=yes) |
| 0x11 | HR17 | uint16 | Nozzle engaged (1=yes) |
| 0x12 | HR18 | uint16 | Weight stable (1=yes) |
| 0x13–0x17 | HR19–HR23 | — | TxnCount, Uptime, Command, DeviceID |

### RTC registers (FC03, start=0x0020, count=6)

| Offset | Description |
|--------|-------------|
| 0x20 | Year |
| 0x21 | Month |
| 0x22 | Day |
| 0x23 | Hour |
| 0x24 | Minute |
| 0x25 | Second |

### Today stats (FC03, start=0x0030, count=6)

| Offset | Description |
|--------|-------------|
| 0x30 | Fills completed today |
| 0x31 | Fills failed today |
| 0x32–0x33 | float32 — kg today |
| 0x34–0x35 | float32 — amount today (PKR) |

### Write registers

| Register | Value | Action |
|----------|-------|--------|
| 0x0017 | 1 | Start fill |
| 0x0017 | 2 | Stop fill |
| 0x0017 | 3 | Reset to Idle |
| 0x0017 | 4 | Zero net weight |
| 0x0006–0x0007 | float32 | Set target weight |
| 0x0008–0x0009 | float32 | Set rate per kg |

---

## 7. Load Cell Calibration

### 7.1 Wiring

| HX711 pin | KC868-A6 terminal | GPIO |
|-----------|-------------------|------|
| DOUT / DT | IO-1 | GPIO32 |
| SCK / PD_SCK | IO-2 | GPIO33 |
| VCC | 3.3 V or 5 V | — |
| GND | GND | — |

The load cell (4-wire) connects to the HX711 E+/E−/A+/A− terminals.

### 7.2 Single-Point Calibration (quick)

1. Place known weight on scale.
2. Open serial terminal on controller at 115200 baud.
3. Run `tare` to zero with the empty platform.
4. Place a known reference weight (e.g. 10 kg).
5. Read `raw` from `hx` command output.
6. Calculate: `factor = raw_reading / known_kg` (negative if reads negative).
7. Run `cal <factor>` to store.

### 7.3 Two-Point Calibration (accurate)

1. Run `tare` with empty platform.
2. Place low reference weight (e.g. 5 kg). Run `cal2 low 5.0`.
3. Place high reference weight (e.g. 20 kg). Run `cal2 high 20.0`.
4. Both points are persisted to NVS automatically.

### 7.4 Verification

After calibration:
- Run `hx` — check `kg=` matches the weight on the scale.
- Run `sim 12.5` to simulate 12.5 kg on the display — confirm display shows 12.500.
- Run `sim clear` to return to real readings.

### 7.5 Default Factor

The firmware ships with `calibrationFactor_ = -7050.0f`. This is a placeholder and will not read correctly until the scale is calibrated for the specific load cell installed.

---

## 8. Building & Flashing

### 8.1 Controller (Arduino / KC868-A6)

Requires `arduino-cli` with esp32 board package installed.

```powershell
# Build
.\scripts\build_firmware.ps1

# Flash (COM10 is typical for KC868-A6 CH340 USB)
.\scripts\upload_firmware.ps1 -Port COM10
```

Or directly:
```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/lpg_controller
arduino-cli upload -p COM10 --fqbn esp32:esp32:esp32 firmware/lpg_controller
```

### 8.2 Display (ESP-IDF / Waveshare ESP32-S3)

Requires ESP-IDF v5.5.4 installed at `C:\Espressif\frameworks\esp-idf-v5.5.4`.

```powershell
# Activate IDF environment (run once per terminal session)
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\python_env\idf5.5_py3.11_env"
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
. "C:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1"

# Build
cd firmware/lpg_display
idf.py build

# Flash (COM9 is typical for Waveshare USB-C)
idf.py -p COM9 flash

# Monitor serial output
idf.py -p COM9 monitor
```

### 8.3 Important: sdkconfig vs sdkconfig.defaults

`sdkconfig.defaults` is the source of truth for Kconfig settings. `sdkconfig` is auto-generated and **must not be committed**.

If you add a new setting to `sdkconfig.defaults`, you **must delete `sdkconfig`** to apply it:

```powershell
Remove-Item firmware/lpg_display/sdkconfig
idf.py build   # regenerates sdkconfig from defaults
```

Editing `sdkconfig` directly is never persistent — it gets overwritten on reconfigure.

---

## 9. sdkconfig Notes

Critical settings in `sdkconfig.defaults` and why they exist:

| Setting | Value | Reason |
|---------|-------|--------|
| `CONFIG_LV_SPRINTF_USE_FLOAT=y` | y | Kept enabled, but display screens use `display_label_setf()` for float labels because LVGL float formatting has still produced `f` on hardware. |
| `CONFIG_LCD_RGB_ISR_IRAM_SAFE=n` | n | The lvgl_port vsync callback is not in IRAM; enabling the ISR IRAM-safe flag crashes on cache misses. |
| `CONFIG_SPIRAM_USE_MALLOC=y` | y | Allows large LVGL draw buffers to use PSRAM automatically. |
| `CONFIG_FREERTOS_HZ=1000` | 1000 | 1 ms tick resolution needed for accurate LVGL animation timing. |
| `CONFIG_ESP_CONSOLE_USB_CDC=y` | y | Keeps `idf.py monitor` working over the USB-C port. |

---

## 10. Display UI Architecture

### 10.1 Screen files

| Screen class | File | Shown when |
|--------------|------|-----------|
| `DashboardScreen` | `screens/DashboardScreen.cpp` | Idle / Ready states |
| `FillProgressScreen` | `screens/FillProgressScreen.cpp` | Validating / Fast / Slow / Settling |
| `FillCompleteScreen` | `screens/FillCompleteScreen.cpp` | Complete |
| `FaultScreen` | `screens/FaultScreen.cpp` | Fault / Aborted |
| `PinScreen` | `screens/PinScreen.cpp` | Admin PIN entry |
| `SettingsScreen` | `screens/SettingsScreen.cpp` | Admin settings |
| `WifiScreen` | `screens/WifiScreen.cpp` | WiFi configuration |

### 10.2 Theme system

`Theme.h` provides:
- `TC::*` — colour palette functions (`TC::ready()`, `TC::danger()`, `TC::active()`, etc.)
- `TF::*` — font shortcuts (`TF::sm()` = 14px, `TF::lg()` = 20px, `TF::hero()` = 48px)
- `Theme::applyCard()`, `Theme::button()`, `Theme::statusDot()` — widget factories

All colours are dark-theme (background `#0f1117`).

### 10.3 Snapshot change detection

`DashboardScreen::update()` uses `snapChanged()` to skip redraws when nothing changed.  
Float fields use epsilon comparison (`fne()`, threshold 0.005 kg) to avoid constant
redraws from scale noise.

### 10.4 Blink animation

Weight stable indicator blinks during active fill using LVGL `lv_anim_t` on the dot's
opacity (500 ms on / 500 ms off, infinite repeat).  
`startStableBlink()` / `stopStableBlink()` manage the animation lifecycle.

---

## 11. Adding a New Screen

1. Create `screens/MyScreen.h` and `screens/MyScreen.cpp`.
2. Add `Screen::MyScreen` to the `Screen` enum in `ScreenManager.h`.
3. Add a `MyScreen myScr;` static in `ScreenManager.cpp`.
4. Handle it in `ScreenManager::loadScreen()`.
5. Add navigation to it from another screen via `screenManager.navigateTo(Screen::MyScreen)`.
6. Add to `CMakeLists.txt` SRCS list.

Minimum screen skeleton:
```cpp
// MyScreen.h
class MyScreen {
public:
    void build();
    void update(/* args */);
    lv_obj_t* screen() { return scr_; }
private:
    lv_obj_t* scr_ = nullptr;
};

// MyScreen.cpp
void MyScreen::build() {
    scr_ = lv_obj_create(nullptr);
    Theme::applyScreenBg(scr_);
    // ... create widgets
}
```

---

## 12. WiFi Manager

`WifiManager` (display board) manages STA+AP simultaneous mode:

| Feature | Detail |
|---------|--------|
| Saved networks | Up to 5, stored in NVS namespace `wifi_cfg` |
| Hotspot SSID | `LPG-Display` |
| Hotspot password | `lpg12345` |
| Connect timeout | 15 seconds per network |
| Auto-switch | Tries next saved network on disconnect if enabled |
| NVS keys | `ssid0`–`ssid4`, `pass0`–`pass4`, `count`, `ap_en`, `auto_sw` |

---

## 13. Known Issues & Gotchas

| Issue | Root cause | Fix |
|-------|-----------|-----|
| Weights show `f` instead of numbers | LVGL `lv_label_set_text_fmt("%.3f", …)` float formatting is unreliable on the display build | Use `display_label_setf()` from `DisplayFormat.h`, rebuild, and flash COM9 |
| Display crashes when START button pressed | `lv_keyboard_create()` inside LVGL event callback causes stack overflow | Use stepper +/- buttons instead of keyboard widget |
| Screen stuck on self-test / never loads | `snap.valid=false` guard prevented dashboard from ever showing | `ScreenManager::begin()` pre-loads dashboard with empty snapshot before Modbus connects |
| Dashboard flickering every 200 ms | `lv_label_set_text_fmt` called every poll even when data unchanged | `snapChanged()` guard skips update when nothing changed |
| RS485 reads own TX echo | Half-duplex RS485 — TX bytes appear in RX buffer | `uart_flush_input()` **after** `uart_wait_tx_done()`, before reading response |
| RS485 pin swap | Display board UART mapping must follow `docs/BOARD_REFERENCE.md` | GPIO43=TX, GPIO44=RX on display board |
| GT911 touch address | Waveshare board does not pull INT low → GT911 defaults to addr 0x5D, not 0x14 | Use `kTouchAddr = 0x5D` in DisplayConfig.h |
| LVGL switch `LV_PART_INDICATOR \| LV_STATE_CHECKED` deprecation warning | LVGL 8.4 treats enum combos as deprecated | Expected warning, no functional impact |
