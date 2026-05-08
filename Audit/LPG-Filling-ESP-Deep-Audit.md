# Deep Audit Report — LPG-Filling-ESP (`codex/firmware-ui-history`)

**Repository:** `raohassandev/LPG-Filling-ESP`  
**Branch reviewed:** `codex/firmware-ui-history`  
**Audit date:** 2026-05-08  
**Audit scope requested:** firmware, UI, UX, layout flow, completeness, gaps, LAN discovery / auto discovery, modernization, production readiness.

> Important note: This audit is based on the repository content visible through GitHub web pages. I could not perform a local clone/build/test from the execution container because DNS resolution to GitHub failed there. I still inspected the repository structure, firmware README, app structure, app router, app package file, app API layer, and app state management exposed by the branch.

---

## 1. Executive Summary

The project has moved beyond a simple prototype. It now has:

- ESP32/KC868-A6 firmware structure for LPG filling control.
- Operator/Admin/Manufacturer Android-first Expo app.
- REST API endpoints.
- WebSocket/MQTT/REST status-stream fallback concept.
- User roles and token-based authentication.
- Transaction logging.
- Rate/amount/weight calculation.
- Modbus TCP map.
- SD/SPIFFS concepts.
- MQTT settings.
- Diagnostics and calibration screens.
- Safety-oriented relay off behavior documented.

However, it is **not yet production complete** for an LPG filling station. It should be treated as a **prototype / engineering validation baseline**, not a deployable commercial system.

Main blockers:

1. **Real hardware validation is incomplete.**
2. **HX711/load cell calibration is still not fully proven on actual wiring.**
3. **Input truth table and safety interlocks are not finalized.**
4. **LAN auto-discovery is weak/incomplete from the mobile operator point of view.**
5. **Security is partially implemented but still not production hardened.**
6. **There is no clear industrial commissioning workflow.**
7. **No formal tests, CI, safety simulation, or acceptance matrix are visible.**
8. **UI is modernizing, but navigation, industrial UX, offline states, and error recovery need improvement.**
9. **Firmware and app are coupled by API expectations but no strict API contract/schema is enforced.**
10. **Modbus RTU is planned but not final; for industrial integration this is a major gap.**

Overall rating:

| Area | Status | Rating |
|---|---:|---:|
| Firmware architecture | Good prototype foundation | 7/10 |
| Filling process logic | Needs real safety validation | 5/10 |
| UI modernism | Improving, Android-first | 7/10 |
| UI/UX industrial usability | Needs clearer workflows | 5/10 |
| LAN discovery | Present conceptually, not enough | 4/10 |
| Security | Better than many prototypes, not production | 5/10 |
| Modbus/HMI integration | TCP basic, RTU not final | 5/10 |
| Calibration | UI exists, hardware validation pending | 4/10 |
| Data logging | Good start, needs audit-grade design | 6/10 |
| Production readiness | Not ready | 4/10 |

---

## 2. Repository Structure Observed

Top-level repository includes:

```text
apps/lpg-expo-app
docs
firmware
scripts
tools
AI-Context.md
DEPLOYMENT.md
DRY_TEST_CHECKLIST.md
LPG_Filling_Station_Development_Plan.md
Prompt.md
SAFETY.md
SECURITY.md
kc868_a6_hardware_profile.md
kc868_a6_professional_plan.md
manual_download_commands.txt
```

Firmware folder contains:

```text
firmware/lpg_controller
firmware/lpg_display
```

Firmware controller folder includes many important modules:

```text
AuthService.h
BoardConfig.h
EventLog.h
FillController.h
InputExpander.h
InputTruthTable.h
ModbusRegisterMap.h
ModbusRtuService.h
ModbusTcpService.h
MqttService.h
NetworkManager.h
OledDisplay.h
RelayBank.h
RtcService.h
SdService.h
SettingsStore.h
StatusStore.h
TransactionLog.h
UserStore.h
WebPortal.h
WeightService.h
lpg_controller.ino
partitions.csv
```

Expo app folder includes:

```text
apps/lpg-expo-app/App.js
apps/lpg-expo-app/package.json
apps/lpg-expo-app/src/api.js
apps/lpg-expo-app/src/components
apps/lpg-expo-app/src/constants
apps/lpg-expo-app/src/hooks
apps/lpg-expo-app/src/screens
apps/lpg-expo-app/src/services
apps/lpg-expo-app/src/state
apps/lpg-expo-app/src/theme.js
apps/lpg-expo-app/src/utils
```

Screens observed:

```text
AdminUsersScreen.js
CalibrationScreen.js
DiagnosticsScreen.js
FaultScreen.js
FillCompleteScreen.js
FillProgressScreen.js
LoginScreen.js
NetworkSettingsScreen.js
OperatorDashboard.js
TransactionsScreen.js
```

This is a good modular direction. The app is no longer one big file only. It has state, services, hooks, screens, components, constants, and theme separation.

---

## 3. Firmware Audit

### 3.1 Firmware Current Scope

The firmware README describes the current scope as including:

- KC868-A6 I2C addresses and relay behavior.
- Safe boot path with all relays forced inactive.
- STA mode with fallback AP.
- OLED IP/status display.
- Serial diagnostics.
- Status API.
- Minimal firmware-hosted landing page.
- Android Expo app support.
- Simulated weight path.
- Serial commands.
- Persistent AP and slow-fill settings.
- Local event log.
- SPIFFS transaction records.
- Tare/net/price calculation.
- Admin rate setup and sales statistics.
- Modbus/HMI register map.
- Modbus TCP server on port 502.

This is a strong prototype scope.

### 3.2 Firmware Explicitly Not Complete

The repo itself documents these incomplete items:

- HX711 calibration on actual KC868-A6 wiring.
- RTC hardware validation.
- Finalized input truth table.
- Production security.
- Finalized network mode policy.

These are not minor items. For LPG automation, they are production blockers.

### 3.3 Safety-Critical Logic

The firmware documents:

- Relay 1 / Output 1: fast fill valve.
- Relay 2 / Output 2: slow fill valve.
- Relay 3 / Output 3: main supply valve.
- Relay 4 / Output 4: pump / compressor.
- Relay 5 / Output 5: alarm horn / beacon.
- Relay 6 / Output 6: status indicator.

Fill sequence:

- Fast fill energizes Relay 1 and Relay 3.
- Slow fill turns Relay 1 off and energizes Relay 2 and Relay 3.
- Stop, complete, abort, and fault force all relays off.
- Thresholds and transaction totals use net weight, not gross/live weight.
- Amount mode target weight = `amount / ratePerKg`.
- Weight mode target amount = `weight * ratePerKg`.

This is correct conceptually, but LPG systems need more than relay sequencing.

Missing safety requirements:

1. Emergency stop input must be hardwired and software-monitored.
2. No-fill condition must be detected if weight does not increase after valve open.
3. Overfill must immediately close all valves.
4. Load cell failure must stop filling.
5. Negative weight drift must be detected.
6. Valve stuck-open/stuck-closed detection is needed.
7. Pump/compressor permissive feedback is needed.
8. Cylinder presence detection should be considered.
9. Door/panel/service mode interlock should be considered.
10. Power-loss recovery must not resume filling automatically.
11. After reboot, all relays must remain off until operator explicitly confirms safe state.
12. Watchdog reset behavior must be defined.
13. E-stop reset should require manual acknowledgement.
14. Calibration mode must lock out filling.
15. Maintenance/manual relay test must be restricted and time-limited.

### 3.4 Input Truth Table

The repo already states the finalized input truth table is not complete. For an industrial LPG filling controller this table is essential.

A proper truth table should include at least:

| Input | Type | Normal state | Fault state | Action |
|---|---|---:|---:|---|
| Emergency stop | DI | Closed/OK | Open/tripped | Stop all relays |
| Nozzle switch | DI | Ready | Not ready | Prevent start |
| Cylinder presence | DI | Present | Absent | Prevent start/stop |
| Pump feedback | DI | Running when commanded | Not running | Stop/fault |
| Fast valve feedback | DI | Matches command | Mismatch | Fault |
| Slow valve feedback | DI | Matches command | Mismatch | Fault |
| Main valve feedback | DI | Matches command | Mismatch | Fault |
| Load cell stable | Internal | Stable | unstable | Delay/stop |
| Overweight | Internal | below limit | above limit | Stop/fault |
| Network state | Internal | connected or local | offline | Continue safe local operation |
| RTC valid | Internal | valid | invalid | Mark transaction uncertain |

The app and firmware should show these states clearly.

### 3.5 Weight/HX711/Calibration

The project includes calibration screens and weight service concepts, but actual wiring/calibration is still marked incomplete.

For a filling machine, the load cell is the main measuring instrument. Weakness here can cause financial loss, inaccurate fills, and safety issues.

Required improvements:

- Store calibration version and date.
- Store calibration operator/admin name.
- Prevent filling if calibration is missing or invalid.
- Support two-point calibration as default.
- Require stable raw readings before accepting calibration point.
- Show raw HX711 reading, filtered reading, live kg, tare kg, net kg.
- Support calibration certificate data entry.
- Add drift check with empty platform.
- Add noise threshold check.
- Add overload detection.
- Add zero tracking only when idle, not during filling.
- Add physical seal/calibration lock concept for commercial use.

### 3.6 Modbus

Firmware documents Modbus TCP support:

- Port 502.
- Function 0x03 for holding register reads.
- Function 0x06 for single register write.
- Writable register `0x1002` tare weight, kg x 100.
- RTU planned after final RS485 baud/parity/HMI wiring.

Current register map:

| Register | Meaning |
|---|---|
| `0x1001` | live weight kg × 100 |
| `0x1002` | tare weight kg × 100 |
| `0x1003` | net weight kg × 100 |
| `0x1004` | filling status code |
| `0x1005` | target weight kg × 100 |
| `0x1006` | E-stop status |

This is too small for industrial integration.

Recommended additional registers:

| Register group | Required data |
|---|---|
| Status | firmware version, device ID, mode, alarm code, warning code |
| Process | live kg, tare kg, net kg, target kg, remaining kg, rate, amount |
| Filling | state, fast valve command, slow valve command, main valve command, pump command |
| Inputs | all DI states, E-stop, nozzle, cylinder, pump feedback |
| Outputs | all DO command states and feedback if available |
| Transactions | last transaction ID, final kg, final amount, operator ID, timestamp |
| Calibration | factor, raw value, zero raw, calibration valid flag |
| Commands | start by kg, start by amount, stop, reset, tare, zero |
| Network | IP octets, Wi-Fi RSSI, connection mode |
| Storage | SD ready, free space, log status |
| Security | login/session status should not be exposed directly, but role-limited command controls needed |

Modbus writes should be disabled by default and enabled only in commissioning mode. The repo security notes already mention production flags for Modbus writes; that is good.

### 3.7 Storage and Transactions

Good direction:

- Transaction records are stored.
- API exposes transaction JSON and CSV.
- App has transaction screen.
- User transactions can be fetched.

Still needed:

- Transaction integrity: sequence number, CRC/hash, no overwrite.
- Power failure transaction recovery.
- Duplicate transaction handling.
- SD card fallback/primary policy.
- SPIFFS wear control.
- Export with date range.
- Export by operator.
- Daily/monthly totals.
- Local backup before SD format/removal.
- Clock validity marker in each transaction.
- Audit log for rate changes, calibration changes, user changes, and manual relay test.

### 3.8 Network Manager and LAN Discovery

The repo indicates:

- STA mode with fallback AP.
- Device-hosted UI possible from SPIFFS.
- Android app default URL is a fixed IP.
- README mentions WebSocket at `ws://<device-ip>/ws`.
- App notes Android `.local` hostnames do not resolve reliably and tells user to use IP from ESP serial monitor.

This is not enough for a field operator.

Required LAN discovery flow:

1. On first app open, show **Find Controller**.
2. Scan likely subnet based on phone Wi-Fi IP.
3. Probe `/api/discover` or `/api/status-public`.
4. Accept devices that return product/device signature.
5. Show list:
   - Device name
   - IP
   - MAC/device ID
   - RSSI if available
   - firmware version
   - role/status
6. Let user select and save.
7. If saved IP fails, auto-rescan.
8. AP onboarding:
   - Connect phone to `LPG-XXXXXX`.
   - App opens `http://192.168.4.1`.
   - User enters site Wi-Fi.
   - Board reconnects to LAN.
   - App searches and reconnects.
9. Optional mDNS:
   - Use `lpg-xxxxxx.local` for browsers/laptops.
   - Do not rely on `.local` on Android as the only method.
10. Optional QR code:
   - Print QR on OLED/web page/label containing AP SSID, AP password, device ID, fallback URL.

Recommended firmware endpoint:

```http
GET /api/discover
```

Example response:

```json
{
  "product": "LPG Filling Controller",
  "model": "KC868-A6",
  "deviceId": "LPG-A6-24ABCD",
  "hostname": "lpg-24abcd",
  "mac": "AA:BB:CC:24:AB:CD",
  "firmware": "0.1.0",
  "apiVersion": 1,
  "ip": "192.168.1.45",
  "apSsid": "LPG-24ABCD",
  "status": "IDLE"
}
```

---

## 4. Expo App Audit

### 4.1 Package and Framework

The app uses:

- Expo `^54.0.0`
- React `19.1.0`
- React Native `0.81.5`
- MQTT `^5.15.1`
- Expo status bar
- React Native Web

Good:

- Modern package versions.
- Android-first app.
- Web support possible.

Concerns:

- No TypeScript.
- No formal lint/test scripts except `node scripts/check-app.js`.
- No AsyncStorage observed, so saved URL/session persistence may be missing.
- No navigation library; manual screen switching is simple but will become fragile.
- No form validation library.
- No schema validation for API responses.
- No offline storage layer.
- No build profiles/EAS config visible in inspected package area.

### 4.2 App Router / Layout Flow

`App.js` is now clean and modular.

Observed route logic:

- If auth loading: show connecting.
- If no token: LoginScreen.
- Firmware state overrides screen:
  - Filling state → FillProgressScreen.
  - Complete state → FillCompleteScreen.
  - Fault state → FaultScreen.
- If idle/manual:
  - transactions
  - admin-users
  - network
  - calibration
  - diagnostics
  - default operator dashboard

This is a good industrial approach because the process state is more important than user navigation. During filling, the operator should not accidentally move away from the active filling screen.

Weaknesses:

1. Manual navigation state is string-based and not deeply structured.
2. No back-stack or navigation history.
3. No role-based route guard shown in router.
4. Firmware state can override calibration/settings screens; this is good during filling, but needs clear warning if user is pushed away.
5. No explicit “commissioning mode” layout.
6. No dedicated “Device Selection / Discovery” screen.
7. No explicit “Lost connection during fill” screen.
8. No “read-only mode” for guest/monitoring.
9. No kiosk/large touch display mode.
10. No language/localization structure.

Recommended route model:

```text
Unauthenticated
 ├── DeviceDiscoveryScreen
 ├── LoginScreen
 └── FirstBootAdminPasswordHelpScreen

Authenticated
 ├── Operator
 │   ├── Dashboard
 │   ├── ActiveFill
 │   ├── FillComplete
 │   └── Fault
 ├── Admin
 │   ├── Users
 │   ├── RateSettings
 │   ├── Reports
 │   ├── Transactions
 │   └── Network
 └── Maintenance/Manufacturer
     ├── Diagnostics
     ├── Calibration
     ├── Modbus
     ├── IO Test
     ├── Relay Test
     ├── System Resources
     └── Firmware/About
```

### 4.3 Status Stream

The app API layer has a good concept:

1. WebSocket on port 81.
2. MQTT over WebSocket fallback.
3. REST `/api/status` polling every 1 second.

This is technically modern and useful.

Concerns:

- WebSocket public payload is described as unauthenticated/minimal.
- MQTT public broker presets are convenient for demo but not suitable for production without strong topic isolation and credentials.
- REST polling every second is acceptable locally but should be adaptive.
- No message sequence number or stale-data indicator is evident.
- No device clock sync validation in the app flow.
- No explicit reconnection banner state design beyond `streamMode`.

Recommended:

- Add `status.seq`.
- Add `status.updatedAtDeviceMs` and app receive timestamp.
- Show **STALE DATA** if no update for >3 seconds.
- Show **CONTROL DISABLED — OFFLINE** if no update for >5 seconds.
- Keep STOP button available if any local route still works.
- Use exponential backoff when idle.
- Use 250–500 ms status push during active filling if firmware can handle it.
- Separate public status from authenticated full status.
- Make MQTT optional and production-disabled until configured with private broker credentials.

### 4.4 Auth and Session Handling

Good:

- Bearer token support exists.
- Login/logout functions exist.
- Roles exist.
- User management screen exists.
- DEV auto-login exists for development.

Concerns:

- No persistent session storage observed.
- DEV auto-login must never be enabled in production.
- Query-string token usage should be removed fully, not just deprecated.
- Login should rate-limit failed attempts.
- App should clearly show role and logged-in operator.
- App should auto-lock after inactivity.
- App should require password/PIN for admin/maintenance actions.
- Password policy and user audit trail should be visible to admin.
- No TLS/HTTPS for local ESP32 is expected, but this means LAN must be trusted and production network policy must be strict.

### 4.5 Operator UI / UX

Operator screen should be simple and safe. For LPG filling, operator needs:

- Big live weight.
- Big net weight.
- Target kg/amount.
- Rate per kg.
- Start/Stop buttons.
- Tare/zero functions.
- Clear cylinder/fill state.
- Alarm/fault display.
- Last transaction.
- Connection status.

Likely good direction:

- Screens exist for dashboard, fill progress, complete, fault.
- The old one-file app had cylinder animation concepts; branch now has split screens, which is better.

Required UX improvements:

1. **Large industrial buttons.**
   - STOP must be red, large, and always visible during filling.
   - START should require confirmation.
2. **Prevent accidental start.**
   - Require target value.
   - Require stable zero/tare.
   - Require E-stop OK.
   - Require rate configured.
   - Require no active fault.
3. **Clear state words.**
   - IDLE
   - READY
   - FAST FILL
   - SLOW FILL
   - SETTLING
   - COMPLETE
   - FAULT
   - OFFLINE
4. **Use color carefully.**
   - Green: ready/complete.
   - Blue/teal: running.
   - Amber: warning/slow fill/settling.
   - Red: stop/fault/E-stop/offline.
5. **Number formatting.**
   - Weight: `00.000 kg`.
   - Amount: `PKR 0,000.00`.
   - Rate: `PKR/kg`.
6. **Operator mistake prevention.**
   - Disable rate change for normal operator unless allowed.
   - Require admin for rate edit.
   - Require confirmation for tare if cylinder is loaded.
7. **Offline during filling.**
   - Show last known data with timestamp.
   - Do not show stale data as live.
8. **Receipt/transaction completion.**
   - Show transaction ID.
   - Show operator.
   - Show time.
   - Show final kg and final amount.
   - Add print/export support later if needed.

### 4.6 Admin UI

Admin should manage:

- Users.
- Roles.
- Rate.
- Sales totals.
- Transaction reports.
- Wi-Fi/network.
- Storage.
- Time/RTC.
- Export CSV.

The branch has user management, transactions, network settings, and settings concepts. That is good.

Gaps:

- No clear reports dashboard by day/month/operator.
- No audit log screen.
- No rate-change history screen.
- No backup/export workflow.
- No SD card health screen in admin flow.
- No business settings page separated from network/technical settings.
- No tax/receipt/company profile settings if this will become commercial.

### 4.7 Manufacturer / Maintenance UI

Manufacturer role should not only show diagnostics. It should support commissioning:

- IO monitor.
- Relay test with timed safety.
- Load cell raw readings.
- Calibration wizard.
- Modbus settings.
- MQTT settings.
- RTC sync.
- Storage diagnostics.
- Network diagnostics.
- Firmware version.
- Factory reset.
- Backup/restore settings.
- Device ID configuration.
- Hardware test checklist.

The branch has diagnostics and calibration screens, but it needs a guided commissioning workflow.

Recommended screen flow:

```text
Maintenance Home
 ├── Hardware Health
 │   ├── Inputs
 │   ├── Outputs
 │   ├── Relays
 │   ├── I2C devices
 │   ├── HX711
 │   └── RTC/SD
 ├── Calibration Wizard
 ├── Modbus Setup
 ├── Network Setup
 ├── MQTT Setup
 ├── Storage/Logs
 ├── Safety Test
 └── Final Commissioning Report
```

---

## 5. LAN Discovery / Auto Discovery Deep Review

### 5.1 Current Situation

The README/app design still depends heavily on a default URL such as:

```text
http://192.168.0.108
```

The app allows changing URL manually. It also tries WebSocket/MQTT/REST for status streaming.

This is usable for a developer but not for a filling station operator.

### 5.2 Problem

In the field:

- Router DHCP may assign different IP.
- Phone may be on another subnet.
- Android often fails `.local`.
- Customer will not open serial monitor.
- Operators will not know IP address.
- Multiple controllers may exist at one site.
- AP fallback may confuse the app if phone remains on mobile data.
- Captive portal behavior varies by Android version.

### 5.3 Required Discovery Features

Add an app screen:

```text
Find Controller
[ Scan LAN ]
[ Connect using IP ]
[ Connect to Setup AP ]
[ Scan QR Code ]
```

LAN scanning method:

1. Detect phone local IP/subnet if possible.
2. Probe `192.168.x.1–254` on port 80 with short timeout.
3. Request `/api/discover`.
4. Validate response signature.
5. Show devices.

Alternative lightweight firmware support:

```http
GET /api/status-public
```

But `discover` is cleaner.

### 5.4 mDNS

Firmware should advertise:

```text
_lpg-controller._tcp.local
hostname: lpg-xxxxxx.local
port: 80
txt:
  model=KC868-A6
  id=LPG-xxxxxx
  fw=0.1.0
```

But app should not depend only on mDNS.

### 5.5 QR Onboarding

Recommended QR payload:

```json
{
  "type": "lpg-controller",
  "deviceId": "LPG-24ABCD",
  "apSsid": "LPG-24ABCD",
  "apPassword": "LP24ABCD",
  "fallbackUrl": "http://192.168.4.1",
  "hostname": "lpg-24abcd.local"
}
```

This avoids manual IP typing.

---

## 6. Security Audit

### 6.1 Good Existing Direction

The repo security policy states:

- No default credentials.
- First boot generates admin account with random 8-character password printed once to serial.
- Passwords use SHA-256 with per-user random salt.
- Session tokens are 128-bit random values using `esp_fill_random`.
- Tokens are sent via `Authorization: Bearer`.
- Query string token fallback is deprecated.
- Role model exists.
- AP password derived from MAC and not shared by all devices.
- No Wi-Fi credentials committed.
- CORS open for local tooling.
- Production flags exist for Modbus writes and dev simulation endpoints.

This is much better than common hobby ESP projects.

### 6.2 Remaining Security Gaps

Security is still not production-grade.

Critical changes:

1. Remove query-string token completely.
2. Disable open CORS in production.
3. Disable dev simulation endpoints in production.
4. Disable DEV auto-login in app production builds.
5. Add login attempt rate limit.
6. Add lockout after repeated failures.
7. Add session expiry warning.
8. Add admin password reset flow.
9. Add password minimum length and policy.
10. Add audit log for:
   - login success/failure
   - user create/delete/block
   - calibration changes
   - rate changes
   - network changes
   - Modbus writes
   - relay manual tests
11. Add firmware build mode visible in app:
   - DEV
   - COMMISSIONING
   - PRODUCTION
12. Add backup recovery for lost admin password.
13. Add physical jumper or long-press reset for controlled factory reset.
14. Do not use public MQTT broker for production.
15. Use private broker credentials and unique topics per device.
16. Consider signed OTA later.

---

## 7. API Contract Audit

Current app and firmware communicate through REST endpoints like:

```text
GET /api/status
GET /api/settings
GET /api/modbus
GET /api/logs
GET /api/transactions
GET /api/transactions.csv
POST /api/start
POST /api/tare
POST /api/tare-zero
POST /api/settings
POST /api/stop
POST /api/reset
POST /api/sim-weight
```

This is a good start.

Missing:

- API versioning.
- JSON schema.
- Error code standard.
- Command idempotency.
- Transaction command acknowledgment.
- Race-condition handling.
- Stale token handling.
- Clear `409 Conflict` when filling state blocks command.
- Clear `423 Locked` for safety interlock.
- Clear `403` for role failure.
- Firmware/app compatibility check.

Recommended response format:

```json
{
  "ok": false,
  "code": "E_STOP_TRIPPED",
  "message": "Emergency stop is not OK",
  "severity": "fault",
  "state": "FAULT"
}
```

Recommended status envelope:

```json
{
  "apiVersion": 1,
  "deviceId": "LPG-24ABCD",
  "seq": 123456,
  "state": "FILLING_SLOW",
  "liveKg": 12.345,
  "tareKg": 4.500,
  "netKg": 7.845,
  "targetKg": 8.000,
  "ratePerKg": 250,
  "amount": 1961.25,
  "readyToFill": false,
  "emergencyStopOk": true,
  "faultCode": "",
  "warningCode": "",
  "updatedAt": 1710000000
}
```

---

## 8. UI/UX Modernism

### Good

- Screen split is modern.
- Theme file exists.
- App is Android-first.
- Status stream is modern.
- MQTT fallback is modern.
- Role-based screens exist.
- Firmware-driven state override is correct for process UI.

### Needs Improvement

1. Add professional industrial dashboard design.
2. Add real navigation bar / drawer / tabs.
3. Add large safe touch targets.
4. Add consistent spacing and typography.
5. Add empty/loading/error states for every screen.
6. Add offline banners.
7. Add stale data warning.
8. Add confirmation modals for dangerous actions.
9. Add icons for roles/actions.
10. Add dark/light or high-contrast mode.
11. Add Urdu/English optional labels if used in Pakistan field sites.
12. Add tablet/5-inch display layout option.
13. Add kiosk mode for embedded display.
14. Add print/share/export transaction.
15. Add commissioning wizard.

### Recommended Visual Layout for Operator Dashboard

```text
┌────────────────────────────────────┐
│ LPG Controller    WS/REST 192.168..│
│ Operator: Ali     State: READY     │
├────────────────────────────────────┤
│ LIVE WEIGHT        15.420 kg       │
│ TARE               05.000 kg       │
│ NET                10.420 kg       │
├────────────────────────────────────┤
│ Target Mode: [Amount] [Weight]     │
│ Amount: PKR [ 2500 ]               │
│ Rate:   PKR/kg 250                 │
│ Target: 10.000 kg                  │
├────────────────────────────────────┤
│ [ TARE ] [ ZERO NET ]              │
│ [ START FILL ]                     │
│ [ STOP / EMERGENCY STOP ]          │
├────────────────────────────────────┤
│ Last: #000123 10.000kg PKR 2500    │
└────────────────────────────────────┘
```

---

## 9. Completeness Matrix

| Feature | Present | Complete? | Notes |
|---|---:|---:|---|
| ESP32/KC868-A6 firmware baseline | Yes | Partial | Good start |
| Relay mapping | Yes | Partial | Needs real validation |
| Safe boot relays off | Yes | Needs test proof | Critical |
| Filling fast/slow sequence | Yes | Partial | Needs fault cases |
| Net weight calculation | Yes | Partial | Needs real calibration |
| HX711 integration | Yes | Not complete | Repo says actual calibration pending |
| Two-point calibration | App concept visible | Partial | Needs firmware proof and wizard |
| RTC | Yes | Not complete | Hardware validation pending |
| SPIFFS transactions | Yes | Partial | Needs integrity and power loss handling |
| SD storage | Yes concept | Partial | Needs final storage policy |
| Event log | Yes | Partial | Needs audit-grade coverage |
| REST API | Yes | Partial | Needs schema/versioning |
| WebSocket | Yes concept | Partial | Needs auth/stale/sequence |
| MQTT | Yes | Partial | Public broker presets not production |
| Modbus TCP | Yes | Partial | Register map too small |
| Modbus RTU | Planned | Not complete | Important for HMI/industrial use |
| User roles | Yes | Partial | Needs route guards and audit |
| User management | Yes | Partial | Needs stronger policy |
| Production security | Partial | Not complete | Repo also says incomplete |
| Android app | Yes | Partial | Good UI foundation |
| LAN auto-discovery | Concept only | Not complete | Major UX gap |
| Device-hosted web UI | Possible | Partial | Needs full portal validation |
| Documentation | Yes | Partial | Some paths appear inconsistent |
| Tests | Some script/checklist | Not enough | Need CI + hardware test docs |

---

## 10. Specific Code/Design Findings

### Finding 1 — App router is clean but too simple for future growth

`App.js` uses firmware-driven phases and manual screen switching. This is good for safety but will become hard to maintain as screens grow.

Recommendation:

- Add a lightweight navigation model.
- Add role guards.
- Add explicit state-machine navigation.
- Separate process state override from manual menu navigation.

### Finding 2 — API layer has good fallback strategy

The status stream priority is:

1. WebSocket.
2. MQTT.
3. REST polling.

This is good architecture.

Recommendation:

- Add sequence numbers.
- Add stale data detection.
- Add reconnect backoff.
- Add device identity check.
- Add status age display.
- Authenticate full WebSocket data or split public/private channels.

### Finding 3 — App state is memory-only

No AsyncStorage was observed in AppStateProvider. This means the app may not persist:

- Device URL.
- Last selected device.
- Session.
- Last role/user.
- App preferences.

Recommendation:

- Add `@react-native-async-storage/async-storage`.
- Persist selected controller URL/device ID.
- Do not persist password.
- Persist token only if you implement secure expiry and risk acceptance.
- Prefer re-login on app open for safety.

### Finding 4 — Default device URL is still manual

The app README mentions default controller URL and manual change. This is not user-friendly.

Recommendation:

- Implement LAN scan and QR onboarding before production.

### Finding 5 — Firmware documentation still has old/inconsistent paths

Firmware README refers to a sketch path that appears inconsistent with the current folder naming in some sections.

Recommendation:

- Clean all docs to use one canonical path:
  - `firmware/lpg_controller/lpg_controller.ino`

### Finding 6 — Production security is acknowledged but not finished

The repo has a strong security policy, but also explicitly notes production security is not complete.

Recommendation:

- Make production build a separate flag/profile.
- Add compile-time checks that fail if dev flags are enabled in production.

### Finding 7 — Modbus RTU is still planned

For Pakistan industrial sites, RS485/Modbus RTU is often essential for HMI, PLC, and energy devices.

Recommendation:

- Finalize RS485 hardware.
- Finalize baud/parity/stop bits.
- Implement RTU slave map.
- Add Modbus command gating and role/security policy.
- Add Modbus diagnostic screen in app.

### Finding 8 — No formal acceptance test matrix visible

There is a dry test checklist, but a complete industrial acceptance matrix should be added.

Recommendation:

- FAT checklist.
- SAT checklist.
- Calibration checklist.
- Safety interlock test.
- Power failure test.
- Network failure test.
- Load cell fault test.
- Relay stuck test.
- E-stop test.
- Transaction recovery test.

---

## 11. Highest Priority Gaps

### P0 — Must fix before real LPG filling test

1. Validate all relay outputs on actual KC868-A6.
2. Validate E-stop hardwired input.
3. Confirm all relays OFF on boot, reboot, watchdog, Wi-Fi loss, app loss.
4. Validate HX711 readings and calibration.
5. Add no-weight-increase detection.
6. Add overfill hard stop.
7. Lock out filling during calibration/maintenance.
8. Add physical emergency stop independent of software.
9. Finalize input truth table.
10. Create dry run test with no LPG before live test.

### P1 — Must fix before customer handover

1. LAN auto-discovery.
2. Stable app device selection.
3. Proper role guards.
4. Remove dev auto-login.
5. Remove query-string token fallback.
6. Production CORS policy.
7. Transaction integrity.
8. RTC validation.
9. SD card policy.
10. Modbus RTU finalization.

### P2 — Should improve for professional product

1. Better reports.
2. Print/share transaction receipt.
3. QR onboarding.
4. Commissioning wizard.
5. OTA update plan.
6. Multi-language labels.
7. Tablet/5-inch layout.
8. Private MQTT/cloud dashboard.
9. CI/build checks.
10. API schema validation.

---

## 12. Recommended Implementation Roadmap

### Phase 1 — Stabilize hardware and safety

- Finalize wiring.
- Validate relays.
- Validate inputs.
- Validate E-stop.
- Validate HX711.
- Add watchdog.
- Add safety state machine.
- Add hardware dry test checklist.

### Phase 2 — Stabilize firmware API

- Define `apiVersion`.
- Define `/api/discover`.
- Define status schema.
- Define standard error responses.
- Add sequence/timestamp.
- Add fault/warning codes.
- Add command acknowledgement.

### Phase 3 — Improve app foundation

- Add device discovery screen.
- Add AsyncStorage for selected device.
- Add route guards.
- Add stale/offline UI.
- Add proper navigation.
- Add better loading/error states.
- Add production/dev build config.

### Phase 4 — Industrial integration

- Complete Modbus RTU.
- Expand register map.
- Add HMI documentation.
- Add MQTT private broker support.
- Add reports and exports.
- Add audit logs.

### Phase 5 — Production readiness

- Security hardening.
- FAT/SAT documents.
- Calibration certificate workflow.
- Backup/restore settings.
- Firmware release versioning.
- OTA/signature plan.
- Enclosure/EMC/noise validation.

---

## 13. Recommended File/Code Improvements

### Add these app files

```text
src/screens/DeviceDiscoveryScreen.js
src/services/discoveryService.js
src/services/storageService.js
src/services/schema.js
src/navigation/routes.js
src/utils/formatters.js
src/utils/statusAge.js
src/components/ConnectionBanner.js
src/components/BigEmergencyStopButton.js
src/components/RoleGuard.js
src/components/ConfirmActionSheet.js
```

### Add these firmware endpoints

```text
GET  /api/discover
GET  /api/status-public
GET  /api/api-version
POST /api/time/sync
GET  /api/faults
GET  /api/inputs
GET  /api/outputs
GET  /api/calibration
POST /api/calibration/start
POST /api/calibration/point
POST /api/calibration/finish
POST /api/maintenance/relay-test
GET  /api/audit-log
```

### Add these docs

```text
docs/API_CONTRACT.md
docs/MODBUS_REGISTER_MAP.md
docs/COMMISSIONING_GUIDE.md
docs/FAT_CHECKLIST.md
docs/SAT_CHECKLIST.md
docs/SAFETY_INTERLOCKS.md
docs/CALIBRATION_PROCEDURE.md
docs/LAN_DISCOVERY.md
docs/PRODUCTION_BUILD.md
```

---

## 14. Proposed API Discovery Flow

App start:

```text
1. Load saved device.
2. Try saved device /api/discover.
3. If OK, continue login.
4. If failed, show DeviceDiscoveryScreen.
5. Scan subnet.
6. Show discovered controllers.
7. User selects controller.
8. Save device ID + IP.
9. Login.
```

When saved IP changes:

```text
1. Saved IP fails.
2. Scan LAN.
3. Match same deviceId.
4. Update IP silently.
5. Show "Controller found at new IP".
```

---

## 15. Proposed Production Safety State Machine

```text
BOOT
 └── SELF_TEST
      ├── FAULT if any relay/input/HX711 critical problem
      └── IDLE_SAFE

IDLE_SAFE
 ├── CALIBRATION_LOCK
 ├── MAINTENANCE_LOCK
 └── READY_TO_FILL

READY_TO_FILL
 └── START_REQUESTED
      ├── validate E-stop
      ├── validate calibration
      ├── validate target
      ├── validate rate
      ├── validate stable weight
      └── FILLING_FAST

FILLING_FAST
 ├── FAULT if E-stop/fault/no weight increase/overfill
 ├── FILLING_SLOW near target
 └── STOPPING if user stop

FILLING_SLOW
 ├── FAULT if E-stop/fault/overfill
 ├── SETTLING when target reached
 └── STOPPING if user stop

SETTLING
 ├── COMPLETE if stable
 └── FAULT if unstable/overfill

COMPLETE
 ├── save transaction
 ├── show receipt
 └── IDLE_SAFE after acknowledge

FAULT
 ├── all relays off
 ├── log event
 └── require authorized reset
```

---

## 16. Final Verdict

This repository is a solid engineering prototype and has a good direction. It is much better organized than a simple Arduino sketch because it includes modular firmware services, authentication, roles, an Android app, status streaming, logs, transactions, Modbus TCP, and documented safety intentions.

But it is **not ready for real LPG station deployment** yet.

Before connecting it to actual LPG valves/pump, complete:

1. Real hardware safety validation.
2. Load cell calibration.
3. E-stop and input truth table.
4. Relay fail-safe testing.
5. LAN discovery and setup flow.
6. Security hardening.
7. Transaction integrity.
8. Modbus RTU and expanded registers.
9. Production build separation.
10. FAT/SAT commissioning documents.

The next best step is not adding more UI screens randomly. The next best step is to freeze a clear **API + safety state machine + commissioning checklist**, then update firmware and app against that contract.
