# LPG Display Firmware — Design Plan

## Hardware
- **Board**: ESP32-S3 5" Capacitive Touch Display (800×480, 5-point I2C touch)
- **CPU**: Xtensa LX7 dual-core 240 MHz, 8 MB PSRAM, 16 MB Flash
- **Connectivity**: Wi-Fi 2.4 GHz, BLE 5, RS485 (Modbus RTU to KC868-A6)
- **Interfaces**: CAN, I2C (touch), UART (RS485), USB-C, TF card, onboard RTC
- **Power**: 7–36 V DC

## Role
Dedicated operator touch interface. Connects to the KC868-A6 (`lpg_controller`) via
**Modbus RTU over RS485 (Serial2)**. No web server, no MQTT — pure local display.

---

## Architecture

```
lpg_display.ino
│
├── Bsp.cpp              — board-specific LVGL + touch init (fill once per board variant)
├── ModbusClient.cpp     — RTU master, polls KC868-A6 every 200 ms, decodes registers
├── ScreenManager.cpp    — state-machine router: switches LVGL screens on state change
│
└── screens/
    ├── DashboardScreen  — Idle / Ready: live weight, readiness, start-fill button
    ├── FillProgressScreen — Fast/Slow/Settling: arc progress, net weight, stop button
    ├── FillCompleteScreen — summary, amount, next-fill button
    ├── FaultScreen        — fault/aborted reason, reset button
    ├── PinScreen          — 4-digit PIN entry for admin access
    └── SettingsScreen     — Calibration | Diagnostics | About (admin only)
```

---

## Screen Flow

```
Boot
 └─→ DashboardScreen (Idle/Ready)
       ├─ state = Fast/Slow/Settling ──→ FillProgressScreen
       │     ├─ state = Complete     ──→ FillCompleteScreen ──→ Dashboard
       │     ├─ state = Fault/Abort  ──→ FaultScreen        ──→ Dashboard
       │     └─ [STOP] confirmed     ──→ Dashboard
       ├─ state = Fault              ──→ FaultScreen
       └─ [ROLE] button              ──→ role/PIN modal
```

---

## Theme — Dark Industrial (mirrors Expo app)

| Token       | Hex       | Usage                          |
|-------------|-----------|--------------------------------|
| bg          | #0f1117   | Root background                |
| surface     | #1a1d27   | Cards, panels                  |
| surface2    | #222633   | Nested panels, inputs          |
| border      | #2a2d3a   | Card borders, dividers         |
| text        | #f0f2f5   | Primary text                   |
| textSub     | #9ca3af   | Labels, units                  |
| ready       | #10b981   | OK, connected, complete        |
| warning     | #f59e0b   | Slow fill, settling, caution   |
| danger      | #ef4444   | Fault, e-stop, abort           |
| active      | #3b82f6   | Fast fill, buttons, active     |
| settling    | #8b5cf6   | Settling state                 |
| muted       | #6b7280   | Offline, inactive indicators   |

---

## Modbus Register Usage

KC868-A6 slave address: **1**  
Baud: **9600 8N1**  
Protocol: FC03 read holding registers, FC06 write single register

| Poll group | Registers | Content |
|------------|-----------|---------|
| A (fast, 200 ms) | 0x0000–0x0017 | Weights (FLOAT32), state, flags, command |
| B (slow, 2 s)    | 0x0020–0x0027 | RTC time |
| C (slow, 5 s)    | 0x0028–0x0047 | Statistics |

**FLOAT32 decode**: two consecutive uint16 registers, Hi word at lower address.  
`bits = (regHi << 16) | regLo; memcpy(&f, &bits, 4);`

**Write command**: FC06 to register 0x0017 (kHR_Command)  
`1 = Start, 2 = Stop, 3 = Reset, 4 = ZeroNet`

---

## Screen Layouts (800 × 480)

### DashboardScreen
```
┌──────────────────────────────────────────────────── 800 ─────┐
│ LPG FILLING STATION          IDLE    WiFi  12:34  [OPERATOR] │ 58
├──────────────────────────────────────────────────────────────┤
│ ┌──────────────────────────────────┐ ┌──────────────────────┐│
│ │ LIVE WEIGHT                       │ │ READINESS            ││
│ │                                   │ │                      ││
│ │       25.340 kg                   │ │ ● E-STOP OK          ││
│ │                                   │ │ ● CYLINDER           ││
│ │  Tare 15.000   Net  10.340 kg     │ │ ○ NOZZLE             ││
│ └──────────────────────────────────┘ │ ○ STABLE             ││
│                                       └──────────────────────┘│
│ ┌──────────────────────────────────┐                          │
│ │ TODAY  12 fills  45.2 kg  5,424₨ │    [ START FILL ]       │
│ └──────────────────────────────────┘                          │
└──────────────────────────────────────────────────────────────┘
```

### FillProgressScreen
```
┌──────────────────────────────────────────────────────────────┐
│ FILLING                          ◉ FAST      12:34           │ 52
├──────────────────────────────────────────────────────────────┤
│                                                              │
│                    8.450 kg                                  │
│              ┌──────────────────────────┐                   │
│              │ ████████████████░░░░░░░  │  84.5%            │
│              └──────────────────────────┘                   │
│                   Target: 10.000 kg                         │
│                                                              │
│  Rate: 120.00 ₨/kg    Amount: 1,014.00 ₨ / 1,200.00 ₨      │
│                                                              │
│                     [ STOP FILL ]                            │
└──────────────────────────────────────────────────────────────┘
```

### FillCompleteScreen
```
┌──────────────────────────────────────────────────────────────┐
│ LPG FILLING STATION                           12:34          │ 52
├──────────────────────────────────────────────────────────────┤
│                        ✓ COMPLETE                            │
│                                                              │
│             ┌──────────────────────────────┐                │
│             │  Net Weight   10.340 kg       │                │
│             │  Amount       1,240.80 ₨      │                │
│             │  Rate         120.00 ₨/kg     │                │
│             │  Duration     0:42            │                │
│             └──────────────────────────────┘                │
│                                                              │
│              [ NEW FILL ]       [ DONE ]                     │
└──────────────────────────────────────────────────────────────┘
```

### FaultScreen
```
┌──────────────────────────────────────────────────────────────┐
│ LPG FILLING STATION                           12:34          │ 52
├──────────────────────────────────────────────────────────────┤
│                                                              │
│                       ✕  FAULT                               │
│                                                              │
│              Reason: weight_timeout                          │
│                                                              │
│              Check the nozzle connection and                 │
│              cylinder placement, then reset.                 │
│                                                              │
│                       [ RESET ]                              │
└──────────────────────────────────────────────────────────────┘
```

---

## File Structure

```
firmware/lpg_display/
├── lpg_display.ino
├── include/
│   ├── DisplayConfig.h      — pin map, default PIN, slave addr
│   ├── Theme.h              — LVGL lv_style_t + lv_color_t constants
│   ├── Bsp.h                — BSP interface (display + touch init)
│   ├── ModbusClient.h       — ControllerSnapshot + polling
│   ├── ScreenManager.h      — screen enum + router
│   └── screens/
│       ├── DashboardScreen.h
│       ├── FillProgressScreen.h
│       ├── FillCompleteScreen.h
│       ├── FaultScreen.h
│       ├── PinScreen.h
│       └── SettingsScreen.h
└── src/
    ├── Bsp.cpp
    ├── ModbusClient.cpp
    ├── ScreenManager.cpp
    └── screens/
        ├── DashboardScreen.cpp
        ├── FillProgressScreen.cpp
        ├── FillCompleteScreen.cpp
        ├── FaultScreen.cpp
        ├── PinScreen.cpp
        └── SettingsScreen.cpp
```

---

## Libraries Required (Arduino IDE)

| Library | Version | Purpose |
|---------|---------|---------|
| lvgl | 8.x | UI framework |
| Board BSP (vendor-specific) | — | Display driver + touch driver init |

Install LVGL 8.x from Arduino Library Manager.  
For the board BSP: check the manufacturer's GitHub for your exact variant.  
A `lv_conf.h` file must be placed in the Arduino `libraries/` folder root.

---

## Admin PIN
Default PIN: `1234` (change in `DisplayConfig.h → kAdminPin`).  
PIN is stored in firmware only — not synced to controller user database.

---

## Implementation Status

- [x] Plan document
- [x] DisplayConfig.h
- [x] Theme.h
- [x] Bsp.h / Bsp.cpp (stub — fill display driver init)
- [x] ModbusClient.h / .cpp (correct register map + IEEE-754 float decode)
- [x] ScreenManager.h / .cpp
- [x] DashboardScreen
- [x] FillProgressScreen
- [x] FillCompleteScreen
- [x] FaultScreen
- [x] PinScreen
- [x] SettingsScreen
- [ ] lv_conf.h (user must provide per their board BSP)
- [ ] Bsp.cpp display+touch driver wiring (user must fill for their board variant)
