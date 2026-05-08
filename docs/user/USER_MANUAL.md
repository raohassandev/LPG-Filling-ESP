# LPG Filling Station — User Manual

**Controller board:** KC868-A6 (ESP32)  
**Display board:** Waveshare ESP32-S3-Touch-LCD-5 (5" 800×480 touchscreen)  
**Firmware:** v0.1.0  
**Document version:** 1.1  

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Hardware Setup](#2-hardware-setup)
3. [Touchscreen Display](#3-touchscreen-display)
4. [First-Time Configuration](#4-first-time-configuration)
5. [User Roles & PIN System](#5-user-roles--pin-system)
6. [Web Interface — Operator](#6-web-interface--operator)
7. [Web Interface — Admin](#7-web-interface--admin)
8. [Web Interface — Manufacturer](#8-web-interface--manufacturer)
9. [Fill Process](#9-fill-process)
10. [Transaction History](#10-transaction-history)
11. [Settings](#11-settings)
12. [User Management](#12-user-management)
13. [Network & Connectivity](#13-network--connectivity)
14. [Serial Console](#14-serial-console)
15. [Modbus TCP Interface](#15-modbus-tcp-interface)
16. [Troubleshooting](#16-troubleshooting)
17. [Technical Specifications](#17-technical-specifications)

---

## 1. System Overview

The LPG Filling Station automates the LPG cylinder filling process. It consists of two boards:

| Board | Role |
|-------|------|
| **KC868-A6 (Controller)** | Reads load cell, drives solenoid relays, runs the fill state machine, hosts the web portal and Modbus server |
| **Waveshare ESP32-S3-Touch-LCD-5 (Display)** | 5" colour touchscreen showing live weight, fill status, and operator controls via RS485 to the controller |

**System diagram:**

```
[Load Cell] → [HX711] → [KC868-A6 Controller]
                              │  RS485
                              ↕
                        [Display Board] ← Operator touch input
                              │
                         [Solenoid Relays] → Gas flow
                              ↕ Wi-Fi
                        [Web / App / Modbus TCP]
```

---

## 2. Hardware Setup

### 2.1 Board: KC868-A6

| Feature | Detail |
|---------|--------|
| MCU | ESP32 (dual-core 240 MHz) |
| Flash | 4 MB (1.375 MB reserved for SPIFFS) |
| I/O | 6× relay outputs, 6× digital inputs |
| Communication | Wi-Fi 802.11 b/g/n, I²C |
| I²C address — relay expander | 0x24 |
| I²C address — input expander | 0x22 |
| I²C address — OLED | 0x3C |
| I²C address — RTC (DS3231) | 0x68 |

### 2.2 Wiring Summary

| Component | Connection |
|-----------|------------|
| HX711 DOUT | GPIO (configured in firmware) |
| HX711 SCK | GPIO (configured in firmware) |
| Solenoid Valve (fast) | Relay 1 |
| Solenoid Valve (slow) | Relay 2 |
| Cylinder-present sensor | Input 1 |
| Nozzle sensor | Input 2 |
| Emergency stop raw/tripped input | Input 4 |
| Spare relays | Relay 3–6 |
| Spare inputs | Input 3, Input 5–6 |

> **Note:** Relay outputs are **active-low** on this board. The firmware handles this internally.

### 2.3 Power

- Supply: 12 V DC recommended (board accepts 7–30 V)
- The ESP32 module is powered from the board's onboard 3.3 V regulator.

---

## 3. Touchscreen Display

The 5" colour touchscreen is the primary operator interface on the factory floor.

### 3.1 Status Bar (top)

| Element | Description |
|---------|-------------|
| **LPG FILLING STATION** | Title (left) |
| **State badge** (centre) | Current fill state: IDLE / READY / FAST / SLOW / SETTLE / DONE / FAULT |
| **WiFi icon** | Tap to open the WiFi settings screen |
| **HH:MM** | Current time from controller RTC; shows `--:--` until a valid RTC value is received |
| **OPERATOR / ADMIN / MANUFACTURER** | Current role badge — tap to change role (PIN required) |

### 3.2 Live Weight Panel (left)

Shows the **live scale reading** from the load cell connected to the controller.  
Below the large number: **Tare** (empty cylinder weight) and **Net** (gas filled so far).

Below the weight panel, the dashboard shows a three-cell fill setup strip:

| Cell | Action |
|------|--------|
| **TARGET kg** | Tap to enter the target fill weight with the numeric keyboard |
| **RATE PKR/kg** | Tap to enter the sale rate with the numeric keyboard |
| **AMOUNT PKR** | Shows the current fill amount, or today's amount when no fill amount is active |

After entering target and rate, press **START FILL** from the dashboard. The old stepper dialog is not required for normal filling.
If the display is not receiving RS485 data from the controller, pressing **START FILL** shows **Controller offline - Check RS485** instead of silently doing nothing.

The coloured bar on the left edge changes colour with the fill state:
- Grey = Idle
- Blue = Active fill
- Green = Complete
- Red = Fault/Aborted

### 3.3 Readiness Panel (top right)

Four status indicators:

| Indicator | Meaning |
|-----------|---------|
| ⏻ E-STOP OK | Emergency stop circuit is closed (safe). Red dot = e-stop tripped. |
| 🏠 CYLINDER PRESENT | Sensor confirms a cylinder is on the platform |
| 💧 NOZZLE ENGAGED | Filling nozzle is locked onto the valve |
| 🔄 WEIGHT STABLE | Scale reading is steady. **Blinks** while controller is measuring/settling. |

### 3.4 Today's Statistics (bottom left)

| Column | Description |
|--------|-------------|
| **Fills** | Number of completed fills today |
| **kg** | Total kg dispensed today |
| **PKR** | Total revenue today |

### 3.5 START FILL Button (bottom right)

Tap to start filling with the current **TARGET kg** and **RATE PKR/kg** strip values.
The button is **blue** when E-stop is OK and the state allows a start. If RS485 is offline, the tap opens a clear offline warning.

### 3.6 WiFi Settings Screen

Accessible from the WiFi button in the status bar:
- View connection status and current IP address
- Add up to 5 saved networks (SSID + password)
- Tap **ADD** to scan nearby WiFi networks first, select an SSID, or use **MANUAL ENTRY**.
- In the add-network window, SSID and password fields stay above the keyboard; use **BACK** to return without saving.
- Toggle **hotspot** on/off (SSID: `LPG-Display`, password: `lpg12345`)
- Toggle **auto-switch** between saved networks
- Tap any network row to delete it

### 3.7 Role / PIN System

The role badge in the status bar controls what the current user can do. Tapping it opens a role selector first.

| Role | PIN | Access |
|------|-----|--------|
| **OPERATOR** | None | Standard operation — start/stop fills |
| **ADMIN** | `1234` | Settings, admin screen access |
| **MANUFACTURER** | `9999` | Full hardware access |

To change role: tap the role badge, choose a role, then enter PIN only for **ADMIN** or **MANUFACTURER**. During PIN entry, **BACK** returns to the role selector.
To return to OPERATOR: tap the badge again, then tap **OPERATOR**.

---

## 4. First-Time Configuration

### 3.1 Initial Access

On first power-up with no Wi-Fi configuration, the board starts an Access Point:

| Parameter | Value |
|-----------|-------|
| SSID | `LPG-Controller-Setup` |
| Password | `lpgsetup123` |
| IP address | `192.168.4.1` |
| mDNS | `lpg-controller.local` |

Connect to this AP and open `http://192.168.4.1` or `http://lpg-controller.local` in a browser.

### 3.2 Default Credentials

> **Change all passwords immediately after first login.**

| Role | Username | Default Password |
|------|----------|-----------------|
| Operator | `operator` | `1234` |
| Admin | `admin` | `0000` |
| Manufacturer | `manufacturer` | `5678` |

The app login screen defaults the controller URL placeholder to `http://lpg-controller.local` and shows a connection indicator: Connected, Connecting, or Offline. The **Sign In** button is disabled while a sign-in attempt is in progress.

### 3.3 Wi-Fi Setup

1. Log in as **Admin** or **Manufacturer**.
2. Go to **Settings → Wi-Fi**.
3. Enter your router SSID and password.
4. The board reboots and connects as a station. The AP remains active as a fallback.

---

## 5. User Roles & PIN System

### 5.1 Touchscreen Roles (Display Board)

See [§3.7 Role / PIN System](#37-role--pin-system).

### 5.2 Web Portal Roles (Controller Board)

| Role | Level | Capabilities |
|------|-------|-------------|
| **Operator** | 1 | Start/stop fills, view own transaction history, read-only settings |
| **Admin** | 3 | All Operator rights + user management, view all transactions, change Wi-Fi settings |
| **Manufacturer** | 2 | All Admin rights + calibration, hardware diagnostics, system resources, firmware update |

**Rate Setting Permission:** Any role can be granted `canSetRate` permission individually by an Admin.

---

## 6. Web Interface — Operator

The Operator screen is available at `http://<board-ip>/` after login.

### 5.1 Dashboard Widgets

| Widget | Description |
|--------|-------------|
| **Live Weight** | Current scale reading in kg |
| **Net Weight** | (Live − Tare) — gas added so far |
| **Fill State** | Current process state (see §8) |
| **Target Weight** | Target fill in kg |
| **Rate/kg** | Price per kg in PKR |
| **Target Amount** | Expected total in PKR |
| **Current Amount** | Running charge (Net × Rate) |
| **Last Transaction** | Most recent completed fill summary |
| **Board Time** | Current RTC time |
| **Network** | Wi-Fi connection status and IP address |

### 5.2 Controls

| Control | Minimum Role | Action |
|---------|-------------|--------|
| **Start Fill** | Operator | Initiates a fill cycle |
| **STOP FILL** | Operator | Aborts the current fill from the fill-progress screen |
| **Reset** | Operator | Resets fault/aborted state to Idle |
| **Zero Net** | Operator | Sets tare to current live weight |
| **Set Rate** | Operator (with canSetRate) | Changes price per kg |

During an active fill, the progress screen shows the phase, animated progress bar, NET kg, TARGET kg, running PKR amount, and a full-width **STOP FILL** button at the bottom.

When a fill completes, the receipt screen shows Transaction ID, net weight dispensed, rate per kg, final amount, duration, and completion time. Tap **New Fill** to reset the controller and return to the dashboard.

The bottom navigation bar is available on Dashboard, History, Settings, Users, Diagnostics, and Calibration. It shows role-appropriate links and a Sign Out action.

---

## 6. Web Interface — Admin

Admins see everything Operators see, plus:

### 6.1 Transaction History

- Full list of all transactions from all operators.
- Transaction rows use a compact single-line format with status dot, transaction ID, kg, final PKR amount, and timestamp.
- Filter by operator username with `?username=<name>` query parameter.
- Export as JSON: `GET /api/transactions`
- Export as CSV: `GET /api/transactions/csv`

### 6.2 User Management

Navigate to **Admin → Users**.

| Action | Endpoint |
|--------|----------|
| List users | `GET /api/users` |
| Create user | `POST /api/users` |
| Update user (password/role/canSetRate/blocked) | `POST /api/users/update` |
| Delete user | `POST /api/users/delete` |

### 6.3 Wi-Fi Settings

`POST /api/settings/wifi` with `{"staSsid":"…","staPassword":"…"}`

---

## 7. Web Interface — Manufacturer

Manufacturers see everything Admins see, plus:

### 7.1 Calibration

- **Two-point calibration** via `/api/calibrate` endpoint.
- Manual calibration factor via serial command `cal <factor>`.

### 7.2 System Resources

| Metric | Description |
|--------|-------------|
| Free Heap | Available RAM in bytes |
| Min Free Heap | Lowest free RAM since boot (watermark) |
| SPIFFS Used / Total | Flash file system usage |
| CPU Frequency | MHz |
| Uptime | Seconds since last boot |
| Chip Temperature | ESP32 internal sensor (approximate) |
| Modbus register map | Live dump of all 25 holding registers |

### 7.3 Hardware Diagnostics

- Raw HX711 readings, calibration factor, scale tare offset.
- Input/relay state table.
- RTC time synchronization.

---

## 8. Fill Process

### 8.1 State Machine

```
BOOT → IDLE → READY → VALIDATING → FILLING_FAST → FILLING_SLOW → SETTLING → COMPLETE
                                                                           ↓
                                                              ABORTED / FAULT
```

| State | Modbus Code | Description |
|-------|-------------|-------------|
| IDLE | 0 | System ready, no fill in progress |
| READY | 1 | Parameters set, waiting for start command |
| VALIDATING | 2 | Checking weight stability, nozzle, cylinder sensors |
| FILLING_FAST | 3 | Fast solenoid open, approaching target |
| FILLING_SLOW | 4 | Slow solenoid only, final approach |
| SETTLING | 5 | Solenoids closed, waiting for weight to settle |
| COMPLETE | 6 | Target reached, transaction saved |
| ABORTED | 7 | Operator stopped or safety limit triggered |
| FAULT | 8 | Sensor fault or system error |
| MAINTENANCE | 9 | Hardware test mode (Manufacturer only) |

### 8.2 Starting a Fill (Web/App)

1. Ensure state is **IDLE**.
2. Set **Target Weight** (kg), **Rate/kg** (PKR), and **Target Amount** (PKR).
3. Press **Start Fill**.
4. The controller validates sensors, then opens the solenoid.
5. Monitor progress on the dashboard.
6. On completion, the transaction is saved automatically.

### 8.3 Emergency Stop

- The current prototype maps E-stop to raw Input 4. When this raw input is active/tripped, all solenoids close immediately and the state transitions to ABORTED/FAULT as appropriate.
- The OLED and web portal both indicate the E-Stop status.

### 8.4 Slow-Fill Threshold

When the net weight reaches **95%** of the target (configurable), the controller switches from fast-fill to slow-fill for accurate final dosing. This threshold is set in firmware (`slowFillThreshold = 0.95`).

---

## 9. Transaction History

### 9.1 Storage

Transactions are stored in **SPIFFS** (onboard flash memory). SPIFFS data **persists across power failures** — it is written to flash before the fill state changes.

| Parameter | Value |
|-----------|-------|
| Storage type | SPIFFS (SPI Flash File System) |
| Partition size | 1.375 MB |
| Per-record size | ~400–450 bytes (one `.txt` file per transaction) |
| Practical capacity | ~2,000–2,500 transactions |
| Power-fail safe | Yes — flash is non-volatile |

### 9.2 Transaction Record Fields

| Field | Type | Description |
|-------|------|-------------|
| id | uint32 | Sequential numeric ID |
| transactionId | string | Human-readable ID (e.g. `TXN-20260501-0042`) |
| startTime | unix timestamp | Fill start time |
| endTime | unix timestamp | Fill end time |
| targetKg | float | Requested fill weight |
| finalKg | float | Actual scale weight at end |
| tareKg | float | Empty cylinder weight |
| netKg | float | Gas dispensed (finalKg − tareKg) |
| ratePerKg | float | Price per kg (PKR) |
| targetAmount | float | Expected charge (PKR) |
| finalAmount | float | Actual charge = netKg × rate |
| status | enum | 0=Pending 1=Complete 2=Aborted 3=Fault |
| reasonCode | string | Abort reason if applicable |
| operator | string | Username of logged-in operator |
| source | string | `api` or `serial` or `modbus` |
| firmwareVersion | string | Firmware version at time of fill |

### 9.3 Export

| Format | Endpoint |
|--------|----------|
| JSON (all) | `GET /api/transactions` |
| JSON (by user) | `GET /api/transactions?username=<name>` |
| CSV | `GET /api/transactions/csv` |

---

## 10. Settings

### 10.1 Available Settings (NVS-persisted)

| Setting | Key | Default |
|---------|-----|---------|
| STA Wi-Fi SSID | `staSsid` | (empty) |
| STA Wi-Fi Password | `staPassword` | (empty) |
| Rate per kg (PKR) | `ratePerKg` | 250.0 |
| Slow-fill threshold | `slowFillThreshold` | 0.95 |

Settings survive reboots (stored in NVS, separate from SPIFFS).

---

## 11. User Management

### 11.1 User Fields

| Field | Description |
|-------|-------------|
| username | Unique, case-insensitive, max 20 chars |
| password | Hashed with djb2 (stored in NVS) |
| role | Operator=1, Maintenance=2, Admin=3 |
| canSetRate | Boolean — allows rate changes even for Operators |
| blocked | Boolean — blocks login without deleting the account |

### 11.2 Limits

- Maximum 20 user accounts.
- Cannot delete own account.
- Cannot block own account.
- Admin role required to create/modify/delete users.

---

## 12. Network & Connectivity

### 12.1 AP Mode (always active)

| Parameter | Value |
|-----------|-------|
| Default SSID | `LPG-Controller-Setup` |
| Default Password | `lpgsetup123` |
| IP (AP) | `192.168.4.1` |

The AP is always active regardless of STA status, giving you a fallback access point if Wi-Fi credentials change.

### 12.2 Station Mode

After Wi-Fi credentials are saved, the board connects to your router. The assigned IP is shown on the OLED and web portal.

### 12.3 mDNS

The board is discoverable at `http://lpg-controller.local` on any device on the same network (requires mDNS/Bonjour).

### 12.4 Port Reference

| Service | Protocol | Port |
|---------|----------|------|
| Web portal | HTTP | 80 |
| Modbus TCP | TCP | 502 |

---

## 13. OLED Status Display

The 128×64 OLED shows four lines:

```
LPG CONTROLLER
STA: 192.168.1.45
AP: 192.168.4.1
STATE: IDLE
```

Updates every 1 second.

---

## 14. Serial Console

Connect at **115200 baud**. The console accepts plain-text commands.

| Command | Description |
|---------|-------------|
| `help` | List all commands |
| `status` | Print full status snapshot |
| `io` / `inputs` | Print raw input bits, configured input mapping, and interpreted readiness |
| `weight` / `hx` | Print HX711 diagnostic data |
| `tare` | Zero the scale at current reading |
| `tarew <kg>` | Set empty cylinder weight |
| `zeronet` | Set tare to current live weight |
| `cal <factor>` | Set HX711 calibration factor |
| `sim <kg>` | Simulate a weight reading (testing) |
| `start <kg> <rate> [amount]` | Start a fill |
| `stop` | Stop current fill |
| `reset` | Reset fault/aborted state to idle |

---

## 15. Modbus TCP Interface

See the separate [MODBUS_PROTOCOL.md](MODBUS_PROTOCOL.md) document for the complete register map, data types, and example sequences.

**Quick reference:**
- Port: **502**
- Protocol: Modbus TCP
- Unit ID: Any (1 recommended)
- Holding Registers: 25 (addresses 40001–40025 in Modbus Poll notation)

---

## 16. Troubleshooting

### Scale not reading correctly

1. Check HX711 DOUT and SCK wiring.
2. Run `hx` in serial console — check `initialized` and `readError` fields.
3. Run `tare` to zero the raw scale.
4. Set `cal <factor>` to correct scale factor.

### Cannot connect to Wi-Fi

1. Board always starts an AP `LPG-Controller-Setup` — connect to it.
2. Log in as Admin, go to Wi-Fi settings.
3. Verify SSID and password (case-sensitive).
4. Check OLED — it shows `STA: CONNECTING` if credentials are set but connection failed.

### E-Stop fault at startup

1. Check that the E-Stop circuit is properly wired normally-closed.
2. The `emergencyStopOk` status will be `false` if raw Input 4 is active/tripped.
3. Use the `io` serial command to confirm raw inputs and interpreted readiness.

### Fill stops early (Aborted)

Check `lastReasonCode` in the status output. Common codes:

| Code | Cause |
|------|-------|
| `estop_triggered` | E-Stop input opened |
| `overweight` | Net weight exceeded target by safety margin |
| `nozzle_lost` | Nozzle sensor lost during fill |
| `serial_stop` | Operator issued `stop` command |
| `api_stop` | Web/app stop button pressed |
| `modbus_stop` | Modbus command register wrote 2 |

### SPIFFS low / transactions not saving

1. Check system resources (Manufacturer panel).
2. If SPIFFS is near full, export transactions via CSV and delete old records manually (no bulk delete endpoint yet — use serial console `reset` and SPIFFS format as last resort).

---

## 17. Technical Specifications

| Parameter | Value |
|-----------|-------|
| MCU | ESP32 dual-core @ 240 MHz |
| Flash total | 4 MB |
| SPIFFS partition | 1.375 MB |
| RAM | 520 KB SRAM |
| Transaction capacity | ~2,000–2,500 records |
| Modbus TCP port | 502 |
| Web portal port | 80 |
| Update mechanism | OTA (two OTA partitions, 1.25 MB each) |
| I²C speed | 100 kHz |
| mDNS hostname | `lpg-controller.local` |
| Firmware version | 0.1.0 |
