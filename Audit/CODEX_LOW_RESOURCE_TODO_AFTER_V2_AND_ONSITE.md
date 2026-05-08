# Codex Low-Resource TODO — After V2 Fix Plan + On-Site Deployment Plan

**Repo/branch:** `raohassandev/LPG-Filling-ESP` / `codex/firmware-ui-history`
**Purpose:** Finish the remaining V2 behavior fixes first, then implement only the minimum on-site deployment configuration needed to reduce developer work during installation.
**Main rule:** Avoid large refactors. Open only the files listed below, make small targeted patches, build once per phase, and update only affected docs.

---

## 1. Current Status After V2 Audit

### 1.1 Already completed or partly completed

The following V2 items appear already present in the repo and should **not be duplicated**:

- Controller/display snapshot now includes `alarmCode`, `alarmSeverity`, `readinessMask`, and `blockerMask`.
- Controller Modbus map already includes diagnostic registers around `0x0048..0x0050`.
- Controller Modbus map already uses RTC Unix timestamp registers at `0x0026/0x0027`.
- Dashboard has partial date/time display.
- Dashboard has partial alarm text mapping.
- Controller has Modbus RTU config registers for slave address, baud, parity, stop bits, and MQTT connected flag.
- Controller scale-readiness gating exists in `FillController::startFill()`.
- Controller already has DS3231 RTC service with initialization and lost-power tracking.
- Controller already has MQTT service behind `LPG_MQTT_ENABLED`.
- Controller already has web routes for WiFi, network, MQTT, time, Modbus RTU, calibration, weight, and system status.

### 1.2 Still not fixed / still weak

These are the remaining high-impact issues:

1. Display RTU timeout is still too low: `kTimeoutMs = 150`.
2. Display RTU settings are still hardcoded in `DisplayConfig.h`:
   - UART1
   - TX GPIO44
   - RX GPIO43
   - baud `9600`
   - slave address `1`
3. Display has no persistent RS485 settings.
4. Display has no `ONLINE / UNSTABLE / OFFLINE` communication health state.
5. `startFill()` still treats one failed RTU write as full failure.
6. `startFill()` does not confirm the controller state after command write.
7. Display can still show false `Controller Offline / Check RS485` during transient RTU misses.
8. `FaultScreen.cpp` still shows generic fault text instead of exact `alarmCode` / `blockerMask` reason.
9. `FillProgressScreen.cpp` still updates labels/bars every refresh and can flicker.
10. `FillProgressScreen.cpp` and `FaultScreen.cpp` show only time, not professional date/time/RTC-invalid status.
11. Input truth table documentation and runtime mapping are still inconsistent.
12. Controller static IP is not yet implemented in settings/network manager.
13. MQTT status payload lacks deployment identity and diagnostic fields.
14. `/api/mqtt/test` is not implemented.
15. `/api/config/export` and `/api/config/import` are not implemented.
16. Commissioning summary API is not implemented.
17. Docs still contain legacy `0x1001` map, old firmware path, and outdated RTU statements.
18. `sdkconfig.defaults` still needs review for ESP32-S3 native USB Serial/JTAG console.

---

## 2. Codex Resource-Saving Rules

Follow these rules strictly to save Codex credits and avoid breaking working code.

### 2.1 Do not re-read the full repo repeatedly

Open only these files first:

#### Display firmware

```text
firmware/lpg_display/main/DisplayConfig.h
firmware/lpg_display/main/ModbusClient.h
firmware/lpg_display/main/ModbusClient.cpp
firmware/lpg_display/main/screens/DashboardScreen.cpp
firmware/lpg_display/main/screens/FillProgressScreen.cpp
firmware/lpg_display/main/screens/FaultScreen.cpp
firmware/lpg_display/sdkconfig.defaults
```

Create these only when needed:

```text
firmware/lpg_display/main/DisplaySettings.h
firmware/lpg_display/main/DisplaySettings.cpp
```

#### Controller firmware

```text
firmware/lpg_controller/include/SettingsStore.h
firmware/lpg_controller/src/SettingsStore.cpp
firmware/lpg_controller/include/NetworkManager.h
firmware/lpg_controller/src/NetworkManager.cpp
firmware/lpg_controller/include/MqttService.h
firmware/lpg_controller/src/MqttService.cpp
firmware/lpg_controller/include/WebPortal.h
firmware/lpg_controller/src/WebPortal.cpp
firmware/lpg_controller/include/ModbusRegisterMap.h
firmware/lpg_controller/src/ModbusRegisterMap.cpp
firmware/lpg_controller/include/InputTruthTable.h
firmware/lpg_controller/src/FillController.cpp
firmware/lpg_controller/include/RtcService.h
firmware/lpg_controller/src/RtcService.cpp
firmware/lpg_controller/lpg_controller.ino
```

Create these only if the existing `SettingsStore` becomes too large:

```text
firmware/lpg_controller/include/DeploymentSettings.h
firmware/lpg_controller/src/DeploymentSettings.cpp
```

#### Controller web UI / docs

Open only if needed:

```text
firmware/lpg_controller/data/index.html
firmware/lpg_controller/data/wifi.html
firmware/lpg_controller/data/modbus.html
firmware/lpg_controller/README.md
firmware/lpg_display/README.md
DEPLOYMENT.md
docs/modbus_and_realtime_protocol.md
```

Add only these docs first:

```text
docs/COMMISSIONING_GUIDE.md
docs/MODBUS_RTU_CURRENT_MAP.md
docs/MQTT_INTEGRATION.md
```

Do **not** generate all possible docs in one pass.

### 2.2 Do not do these things

- Do not overwrite RTC Unix timestamp registers `0x0026/0x0027`.
- Do not reintroduce old `0x1001` Modbus map as active map.
- Do not make ESP32 controller or display an MQTT broker.
- Do not enable MQTT remote start/stop by default.
- Do not make local filling dependent on WiFi or MQTT.
- Do not hard-swap input mapping without bench verification.
- Do not rewrite the complete UI framework.
- Do not modify the mobile app unless a build or API break forces it.
- Do not refactor all settings storage at once.
- Do not export WiFi/MQTT passwords by default.

### 2.3 Build policy

Build after each phase, not after every file:

```bash
# Controller
cd firmware/lpg_controller
arduino-cli compile --fqbn <controller-fqbn> .

# Display
cd firmware/lpg_display
idf.py build
```

If exact FQBN is unknown, do not guess in code. Update the doc with the verified board command already used by the project.

---

## 3. Fast Audit Commands for Codex

Run these once before editing:

```bash
grep -R "kTimeoutMs\|kRtuBaud\|kRtuAddr" firmware/lpg_display/main

grep -R "System fault detected\|faultHint\|lv_label_set_text_fmt(lblTime" firmware/lpg_display/main/screens

grep -R "0x1001\|RTU is not enabled\|kc868_a6_lpg_controller\|password123\|STA SSID" .

grep -R "handleMqtt\|/api/mqtt\|/api/config/export\|/api/config/import\|/api/commissioning" firmware/lpg_controller/src firmware/lpg_controller/include

grep -R "inputState(0)\|inputState(1)\|inputState(2)\|inputState(3)" firmware/lpg_controller/src firmware/lpg_controller/include
```

Use the grep results to patch only affected files.

---

## 4. Phase 1 — Finish V2 Professional Behavior First

**Goal:** Stop false offline popups, show exact fault reasons, reduce flicker, and keep Modbus/RTC map safe.

### 4.1 Add display communication health state

Edit:

```text
firmware/lpg_display/main/ModbusClient.h
firmware/lpg_display/main/ModbusClient.cpp
```

Add:

```cpp
enum class CommHealth : uint8_t {
  Offline = 0,
  Unstable = 1,
  Online = 2,
};
```

Extend `ControllerSnapshot`:

```cpp
CommHealth commHealth = CommHealth::Offline;
uint32_t lastOkUs = 0;
uint32_t lastFailUs = 0;
uint16_t consecutiveOk = 0;
uint16_t consecutiveFail = 0;
```

Use this behavior:

```text
ONLINE   = at least 2 consecutive successful polls
UNSTABLE = at least 1 failed poll but offline debounce not expired
OFFLINE  = no successful poll for offlineDebounceMs
```

Default values:

```cpp
static constexpr uint32_t kDefaultTimeoutMs = 300;
static constexpr uint8_t kDefaultRetries = 2;
static constexpr uint32_t kDefaultUnstableDebounceMs = 800;
static constexpr uint32_t kDefaultOfflineDebounceMs = 3000;
```

Do not show the hard offline popup while `commHealth == CommHealth::Unstable`.

### 4.2 Increase display RTU timeout

Replace:

```cpp
static constexpr uint32_t kTimeoutMs = 150;
```

with either:

```cpp
static constexpr uint32_t kDefaultTimeoutMs = 300;
```

or use the new persistent RTU setting:

```cpp
cfg_.rtu.timeoutMs
```

### 4.3 Add RTU retry wrappers

Add wrappers in `ModbusClient.cpp`:

```cpp
bool ModbusClient::readHrWithRetry(uint16_t addr, uint16_t count, uint16_t* out) {
  for (uint8_t i = 0; i <= cfg_.rtu.retries; ++i) {
    if (readHR(addr, count, out)) return true;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  return false;
}

bool ModbusClient::writeRegWithRetry(uint16_t addr, uint16_t value) {
  for (uint8_t i = 0; i <= cfg_.rtu.retries; ++i) {
    if (writeRegister(addr, value)) return true;
    vTaskDelay(pdMS_TO_TICKS(30));
  }
  return false;
}

bool ModbusClient::writeRegsWithRetry(uint16_t addr, const uint16_t* values, uint16_t count) {
  for (uint8_t i = 0; i <= cfg_.rtu.retries; ++i) {
    if (writeRegisters(addr, values, count)) return true;
    vTaskDelay(pdMS_TO_TICKS(30));
  }
  return false;
}
```

Use retries for polling and commands.

### 4.4 Fix `startFill()` false failure

Current issue: if target/rate/amount/command write has one missed response, UI can show offline even when controller may accept the command.

Required behavior:

1. Write target, rate, amount using retry.
2. Write command using retry.
3. After command write, always read fill state register `0x000E`.
4. Treat command as successful if controller state is one of:
   - `Validating`
   - `FillingFast`
   - `FillingSlow`
   - or any project equivalent active-fill state.
5. If command write failed but state changed to active fill, return success with warning.
6. If command write failed and state did not change, return failure but mark communication as `UNSTABLE`, not immediately `OFFLINE`.

Pseudo-code:

```cpp
bool ModbusClient::confirmFillStarted() {
  uint16_t state = 0;
  if (!readHrWithRetry(0x000E, 1, &state)) return false;
  return state == static_cast<uint16_t>(FillState::Validating) ||
         state == static_cast<uint16_t>(FillState::FillingFast) ||
         state == static_cast<uint16_t>(FillState::FillingSlow);
}

bool ModbusClient::startFill(float targetKg, float ratePerKg, float amountPkr) {
  bool paramsOk = true;
  paramsOk &= writeFloatWithRetry(kHR_TargetKg, targetKg);
  paramsOk &= writeFloatWithRetry(kHR_RatePerKg, ratePerKg);
  paramsOk &= writeFloatWithRetry(kHR_TargetAmount, amountPkr);

  bool commandOk = writeRegWithRetry(kHR_Command, kCmdStartFill);

  if (confirmFillStarted()) {
    return true;
  }

  if (!paramsOk || !commandOk) {
    markCommUnstable();
  }
  return false;
}
```

Use existing enum/register names if different.

### 4.5 Fix fault screen exact reason

Edit:

```text
firmware/lpg_display/main/screens/FaultScreen.cpp
```

Do not show only generic `System fault detected`.

Add:

```cpp
static const char* alarmTitle(uint16_t code) {
  switch (code) {
    case 0: return "No active alarm";
    case 1: return "Emergency stop active";
    case 2: return "Nozzle not engaged";
    case 3: return "Cylinder not detected";
    case 4: return "Scale read error";
    case 5: return "Scale unstable";
    case 6: return "Scale not calibrated";
    case 7: return "Overfill protection active";
    case 8: return "Fill timeout";
    case 9: return "No flow detected";
    case 10: return "Transaction log error";
    case 11: return "Fill stopped";
    case 12: return "Controller fault active";
    default: return "Unknown controller alarm";
  }
}

static const char* alarmHint(uint16_t code, uint16_t blockerMask) {
  switch (code) {
    case 1: return "Release E-stop, verify input, then reset.";
    case 2: return "Check nozzle switch, wiring, and connector.";
    case 3: return "Check cylinder sensor and placement.";
    case 4: return "Check HX711, load cell wiring, and power.";
    case 5: return "Wait until scale becomes stable.";
    case 6: return "Run scale calibration before filling.";
    case 9: return "Weight is not increasing. Check valve, relay output, and gas flow.";
    default:
      return blockerMask ? "Check readiness blockers and reset." : "Check system status and reset.";
  }
}
```

In `update()`:

```cpp
lv_label_set_text(lblReason_, alarmTitle(snap.alarmCode));
lv_label_set_text(lblHint_, alarmHint(snap.alarmCode, snap.blockerMask));
```

### 4.6 Add shared date/time formatting helper

Create or localize a helper used by Dashboard, FillProgress, and Fault screens:

```cpp
static bool rtcLooksValid(const ControllerSnapshot& snap) {
  return snap.rtcYear >= 2024 && snap.rtcYear <= 2099 &&
         snap.rtcMonth >= 1 && snap.rtcMonth <= 12 &&
         snap.rtcDay >= 1 && snap.rtcDay <= 31 &&
         snap.rtcHour <= 23 && snap.rtcMinute <= 59;
}

static void setDateTimeLabel(lv_obj_t* label, const ControllerSnapshot& snap) {
  if (!rtcLooksValid(snap)) {
    setLabelTextIfChanged(label, "RTC NOT SET");
    return;
  }
  setLabelFmtIfChanged(label, "%02u/%02u %02u:%02u",
                       snap.rtcDay, snap.rtcMonth,
                       snap.rtcHour, snap.rtcMinute);
}
```

If shared helpers are too invasive, duplicate this small helper in the three screen files to save time.

### 4.7 Reduce FillProgress flicker

Edit:

```text
firmware/lpg_display/main/screens/FillProgressScreen.cpp
```

Replace direct repeated calls with set-if-changed wrappers.

Minimum helper:

```cpp
static void setLabelTextIfChanged(lv_obj_t* label, const char* text) {
  if (!label || !text) return;
  const char* cur = lv_label_get_text(label);
  if (!cur || strcmp(cur, text) != 0) {
    lv_label_set_text(label, text);
  }
}

template <typename... Args>
static void setLabelFmtIfChanged(lv_obj_t* label, const char* fmt, Args... args) {
  char buf[64];
  snprintf(buf, sizeof(buf), fmt, args...);
  setLabelTextIfChanged(label, buf);
}
```

Only update progress bar if value changed:

```cpp
if (lv_bar_get_value(bar_) != pct1000) {
  lv_bar_set_value(bar_, pct1000, LV_ANIM_OFF);
}
```

Only update style color when state changed. Keep a cached `lastState_` if needed.

### 4.8 Fix ESP32-S3 USB console config if required

Edit:

```text
firmware/lpg_display/sdkconfig.defaults
```

If this display board uses native ESP32-S3 USB Serial/JTAG, use:

```ini
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
# CONFIG_ESP_CONSOLE_USB_CDC is not set
```

Do not change this if the project intentionally uses USB CDC through TinyUSB and bench testing confirms it.

### 4.9 Input mapping: do not blind-fix yet

There is a mismatch:

- Runtime currently appears to use input `0 = cylinder`, `1 = nozzle`, `3 = E-stop tripped`.
- Documentation/truth table says input `0 = nozzle`, `1 = cylinder`, `2 = E-stop`.

For this phase:

1. Add a code comment warning about the mismatch.
2. Add `/api/io/live` in Phase 2 so technician can see raw input bits.
3. Do **not** change live input mapping until bench wiring is verified or configurable mapping is implemented.

---

## 5. Phase 2 — Minimum On-Site Deployment Implementation

**Goal:** Make the most common site parameters configurable without source-code edits.

Use the uploaded on-site plan as product reference. Implement the smallest working subset first.

### 5.1 Add persistent display RTU settings

Create:

```text
firmware/lpg_display/main/DisplaySettings.h
firmware/lpg_display/main/DisplaySettings.cpp
```

Minimum model:

```cpp
#pragma once
#include <stdint.h>

struct DisplayRtuSettings {
  uint8_t slaveAddress = 1;
  uint32_t baudRate = 9600;
  uint8_t parity = 0;       // 0=None, 1=Even, 2=Odd
  uint8_t stopBits = 1;
  uint16_t timeoutMs = 300;
  uint8_t retries = 2;
  uint16_t unstableDebounceMs = 800;
  uint16_t offlineDebounceMs = 3000;
};

struct DisplayIdentitySettings {
  char displayId[24] = "DSP-001";
  char stationId[24] = "LPG-STN-001";
};

struct DisplaySettingsSnapshot {
  uint16_t schemaVersion = 1;
  DisplayIdentitySettings identity;
  DisplayRtuSettings rtu;
};
```

Use ESP-IDF NVS to save/load. If NVS implementation takes too long, start with defaults plus clean TODO, but make `ModbusClient::begin(settings.rtu)` ready now.

### 5.2 Replace display hardcoded RTU use

Edit:

```text
firmware/lpg_display/main/ModbusClient.h
firmware/lpg_display/main/ModbusClient.cpp
firmware/lpg_display/main/DisplayConfig.h
```

Keep hardcoded UART pins in `DisplayConfig.h`, but move address/baud/parity/stop/timeout/retries into persistent settings.

Expected API:

```cpp
void ModbusClient::begin(const DisplayRtuSettings& rtu);
void ModbusClient::applySettings(const DisplayRtuSettings& rtu);
```

UART config:

```cpp
uart_config_t uartCfg = {
  .baud_rate = static_cast<int>(rtu.baudRate),
  .data_bits = UART_DATA_8_BITS,
  .parity = rtu.parity == 1 ? UART_PARITY_EVEN :
            rtu.parity == 2 ? UART_PARITY_ODD : UART_PARITY_DISABLE,
  .stop_bits = rtu.stopBits == 2 ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
  .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  .source_clk = UART_SCLK_DEFAULT,
};
```

Frame generation:

```cpp
req[0] = cfg_.rtu.slaveAddress;
```

Frame validation:

```cpp
return len >= expected && frame[0] == cfg_.rtu.slaveAddress && frame[1] == fn;
```

Timeout:

```cpp
const int64_t deadline = esp_timer_get_time() + static_cast<int64_t>(cfg_.rtu.timeoutMs) * 1000;
```

### 5.3 Add basic display RS485 settings/admin screen

Minimum acceptable UI:

```text
Admin → Controller Link
- Status: ONLINE / UNSTABLE / OFFLINE
- Slave Address
- Baud Rate
- Parity
- Stop Bits
- Timeout ms
- Retries
- Save & Reconnect
- Test Link
```

Do **not** implement full scan yet if time is tight. Add scan in Phase 3.

### 5.4 Add controller static IP settings

Edit:

```text
firmware/lpg_controller/include/SettingsStore.h
firmware/lpg_controller/src/SettingsStore.cpp
firmware/lpg_controller/include/NetworkManager.h
firmware/lpg_controller/src/NetworkManager.cpp
firmware/lpg_controller/src/WebPortal.cpp
```

Add minimum settings:

```cpp
struct IpSettings {
  bool dhcp = true;
  String ip;
  String gateway;
  String subnet;
  String dns1;
  String dns2;
};
```

Add this under network/WiFi settings. Do not build a separate deployment settings store unless necessary.

Apply before `WiFi.begin()`:

```cpp
static IPAddress parseIpOrZero(const String& s) {
  IPAddress ip;
  if (!ip.fromString(s)) return IPAddress(0, 0, 0, 0);
  return ip;
}

bool LpgNetworkManager::applyStaIpConfig(const IpSettings& cfg) {
  if (cfg.dhcp) return true;

  IPAddress ip = parseIpOrZero(cfg.ip);
  IPAddress gw = parseIpOrZero(cfg.gateway);
  IPAddress sn = parseIpOrZero(cfg.subnet);
  IPAddress dns1 = parseIpOrZero(cfg.dns1);
  IPAddress dns2 = parseIpOrZero(cfg.dns2);

  if (ip == IPAddress(0,0,0,0) || gw == IPAddress(0,0,0,0) || sn == IPAddress(0,0,0,0)) {
    Serial.println("[NET] Invalid static IP config; using DHCP");
    return false;
  }

  if (dns1 == IPAddress(0,0,0,0)) dns1 = gw;
  WiFi.config(ip, gw, sn, dns1, dns2);
  return true;
}
```

Expose fields in:

```text
GET  /api/wifi
POST /api/wifi
GET  /api/network
```

### 5.5 Add deployment identity fields

Add to settings:

```cpp
struct DeviceIdentitySettings {
  String stationId = "LPG-STN-001";
  String controllerId = "CTRL-001";
  String siteName = "";
  String nozzleId = "NOZ-01";
};
```

Expose in:

```text
GET  /api/system
GET  /api/commissioning
POST /api/commissioning
```

### 5.6 Add MQTT test endpoint

Edit:

```text
firmware/lpg_controller/include/MqttService.h
firmware/lpg_controller/src/MqttService.cpp
firmware/lpg_controller/src/WebPortal.cpp
```

Add:

```text
POST /api/mqtt/test
```

Expected response:

```json
{
  "ok": true,
  "enabled": true,
  "connected": true,
  "topic": "lpg/LPG-STN-001/CTRL-001/status",
  "message": "test publish sent"
}
```

If MQTT is not compiled:

```json
{
  "ok": false,
  "enabled": false,
  "compiled": false,
  "message": "MQTT support is disabled at build time"
}
```

### 5.7 Improve MQTT payload and topics

Current MQTT status payload is too small for deployment. Add at least:

```json
{
  "deviceType": "lpg-controller",
  "stationId": "LPG-STN-001",
  "controllerId": "CTRL-001",
  "ts": 1770000000,
  "state": "READY",
  "liveKg": 0.0,
  "netKg": 0.0,
  "targetKg": 12.0,
  "amount": 0.0,
  "estopOk": true,
  "cylinderPresent": true,
  "nozzleEngaged": true,
  "weightStable": true,
  "alarmCode": 0,
  "alarmSeverity": 0,
  "readinessMask": 63,
  "blockerMask": 0,
  "uptimeSec": 3600
}
```

Topic format:

```text
<topicPrefix>/<stationId>/<controllerId>/status
<topicPrefix>/<stationId>/<controllerId>/transaction
<topicPrefix>/<stationId>/<controllerId>/alert
<topicPrefix>/<stationId>/<controllerId>/lwt
```

Keep MQTT commands disabled by default.

### 5.8 Add config export/import skeleton

Edit:

```text
firmware/lpg_controller/src/WebPortal.cpp
firmware/lpg_controller/include/SettingsStore.h
firmware/lpg_controller/src/SettingsStore.cpp
```

Add:

```text
GET  /api/config/export
POST /api/config/import
```

Minimum export must include:

```json
{
  "schemaVersion": 1,
  "identity": {},
  "wifi": {},
  "rtu": {},
  "mqtt": {},
  "fill": {},
  "calibration": {},
  "productionLock": false
}
```

Default rule:

```text
Do not export WiFi password or MQTT password.
```

Add optional `includeSecrets=true` only later.

### 5.9 Add commissioning summary API

Add:

```text
GET /api/commissioning
POST /api/commissioning
```

Minimum response:

```json
{
  "ok": true,
  "commissioningComplete": false,
  "identityConfigured": true,
  "networkConfigured": true,
  "rtuConfigured": true,
  "scaleCalibrated": false,
  "inputsVerified": false,
  "mqttConfigured": false,
  "rtcValid": true,
  "productionLocked": false,
  "blockers": ["scaleCalibrated", "inputsVerified"]
}
```

Use this to block filling later when commissioning is incomplete.

---

## 6. Phase 3 — Calibration, Inputs, Relay Test, RTC

Do this after Phase 1 and Phase 2 build cleanly.

### 6.1 HX711 calibration wizard

Use existing endpoints first:

```text
GET  /api/weight
POST /api/tare-hw
POST /api/calibrate
```

Enhance only if needed. Minimum wizard states:

1. HX711 live raw check
2. Empty scale tare
3. Place known weight
4. Enter known weight
5. Save factor
6. Verify reading

Required behavior:

- Fill blocked if calibration invalid.
- Fault screen says `Scale not calibrated`.
- Calibration cannot run during active fill.

### 6.2 Input truth table wizard

Add:

```text
GET  /api/io/live
POST /api/io/mapping
```

Minimum response:

```json
{
  "rawInputs": [true, false, true, false, false, false],
  "mapping": {
    "nozzle": { "channel": 0, "activeHigh": true, "verified": false },
    "cylinder": { "channel": 1, "activeHigh": true, "verified": false },
    "estop": { "channel": 2, "activeHigh": false, "verified": false }
  },
  "interpreted": {
    "nozzleEngaged": true,
    "cylinderPresent": true,
    "estopOk": true
  }
}
```

Then update `FillController::syncInputs()` to use mapping from settings.

### 6.3 Relay pulse test

Add:

```text
POST /api/relay/test-pulse
```

Request:

```json
{
  "relay": 1,
  "durationMs": 1000
}
```

Rules:

- Reject during active fill.
- Max pulse `2000 ms`.
- Maintenance/Admin only if roles exist.
- Force all relays off when test ends.

### 6.4 RTC validity and set from browser time

Add or improve:

```text
GET  /api/time
POST /api/time
```

RTC valid rule:

```cpp
bool RtcService::dateLooksValid(const DateTime& dt) const {
  return dt.year() >= 2024 && dt.year() <= 2099 &&
         dt.month() >= 1 && dt.month() <= 12 &&
         dt.day() >= 1 && dt.day() <= 31;
}
```

If Modbus needs RTC status, add new registers **after current map end**, for example:

```text
0x0051 RTC valid flag
0x0052 RTC lost-power flag
```

But first check `kHR_Count` and increase safely.

---

## 7. Documentation Cleanup TODO

### 7.1 Must update now

```text
firmware/lpg_controller/README.md
DEPLOYMENT.md
docs/modbus_and_realtime_protocol.md
```

Fix:

- Replace old path `firmware/kc868_a6_lpg_controller/...` with `firmware/lpg_controller/lpg_controller.ino`.
- Mark old `0x1001` map as legacy only, or remove it from active docs.
- Document active 0-based Modbus RTU map.
- Remove statement that RTU is not enabled if RTU is active.
- Remove production-looking default credentials such as `Rao/password123` from primary docs.
- Add warning that MQTT broker presets are testing only.

### 7.2 Add small new docs

Add only these first:

```text
docs/COMMISSIONING_GUIDE.md
docs/MODBUS_RTU_CURRENT_MAP.md
docs/MQTT_INTEGRATION.md
```

Do not add every proposed document in one PR. That wastes Codex credits.

---

## 8. Acceptance Tests

### Phase 1 tests

1. Disconnect RS485 briefly.
   - Expected: `UNSTABLE`, not immediate hard `OFFLINE`.
2. Keep RS485 disconnected beyond debounce.
   - Expected: `OFFLINE`.
3. Reconnect RS485.
   - Expected: automatic return to `ONLINE`.
4. Press Start Fill with one simulated missed response.
   - Expected: display confirms controller state before showing failure.
5. Trigger each known alarm.
   - Expected: exact fault reason appears on Fault screen.
6. Stay on FillProgress screen for 2 minutes.
   - Expected: no visible flicker from repeated identical label updates.
7. RTC invalid.
   - Expected: screen shows `RTC NOT SET`, not silent `00:00`.

### Phase 2 tests

1. Display saves RTU baud/address/retries.
   - Reboot display.
   - Expected: same RTU settings reload.
2. Controller static IP.
   - Set static IP and reboot.
   - Expected: device reachable at configured IP.
3. MQTT test.
   - Enable custom broker.
   - Press test.
   - Expected: broker receives payload with `stationId` and `controllerId`.
4. Config export.
   - Expected: JSON downloads and excludes passwords by default.
5. Commissioning summary.
   - Expected: clearly lists incomplete scale/input/MQTT/RTC items.

### Phase 3 tests

1. HX711 calibration survives reboot.
2. Fill is blocked when calibration invalid.
3. Input mapping wizard correctly maps actual wiring.
4. Relay pulse test is blocked during active fill.
5. RTC can be set from browser time.

---

## 9. Minimal Commit Plan

Use these small commits to reduce risk.

### Commit 1 — Finish display RTU professional behavior

Files:

```text
firmware/lpg_display/main/ModbusClient.h
firmware/lpg_display/main/ModbusClient.cpp
```

Changes:

- Timeout 300 ms.
- Retries.
- `CommHealth`.
- Start fill confirmation.
- No hard offline after one failure.

### Commit 2 — Fix fault/progress screens

Files:

```text
firmware/lpg_display/main/screens/FaultScreen.cpp
firmware/lpg_display/main/screens/FillProgressScreen.cpp
firmware/lpg_display/main/screens/DashboardScreen.cpp
```

Changes:

- Exact alarm text.
- RTC valid/date formatting.
- Set-if-changed label updates.

### Commit 3 — Add display RTU settings

Files:

```text
firmware/lpg_display/main/DisplaySettings.h
firmware/lpg_display/main/DisplaySettings.cpp
firmware/lpg_display/main/ModbusClient.h
firmware/lpg_display/main/ModbusClient.cpp
```

Changes:

- Persistent slave address, baud, parity, stop bits, timeout, retries, debounces.

### Commit 4 — Controller static IP and identity

Files:

```text
firmware/lpg_controller/include/SettingsStore.h
firmware/lpg_controller/src/SettingsStore.cpp
firmware/lpg_controller/include/NetworkManager.h
firmware/lpg_controller/src/NetworkManager.cpp
firmware/lpg_controller/src/WebPortal.cpp
```

Changes:

- IP settings.
- Device identity.
- API fields.

### Commit 5 — MQTT test and payload identity

Files:

```text
firmware/lpg_controller/include/MqttService.h
firmware/lpg_controller/src/MqttService.cpp
firmware/lpg_controller/src/WebPortal.cpp
```

Changes:

- `/api/mqtt/test`.
- station/controller identity in topics and payload.
- alarm/readiness/blocker fields.

### Commit 6 — Config export/import and commissioning summary

Files:

```text
firmware/lpg_controller/src/WebPortal.cpp
firmware/lpg_controller/include/SettingsStore.h
firmware/lpg_controller/src/SettingsStore.cpp
```

Changes:

- `/api/config/export`.
- `/api/config/import` skeleton.
- `/api/commissioning`.

### Commit 7 — Docs cleanup

Files:

```text
firmware/lpg_controller/README.md
firmware/lpg_display/README.md
DEPLOYMENT.md
docs/modbus_and_realtime_protocol.md
docs/COMMISSIONING_GUIDE.md
docs/MODBUS_RTU_CURRENT_MAP.md
docs/MQTT_INTEGRATION.md
```

Changes:

- Remove/mark legacy `0x1001` map.
- Correct paths.
- Document active RTU map and commissioning flow.

---

## 10. Copy-Paste Prompt for Codex

```text
You are working in repo raohassandev/LPG-Filling-ESP branch codex/firmware-ui-history.

Task: implement the remaining fixes after V2 and then the smallest on-site deployment configuration set. Use minimum Codex resources. Do not re-read the whole repo repeatedly. Open only the files listed below and make small commits.

Start by auditing these exact files:
- firmware/lpg_display/main/DisplayConfig.h
- firmware/lpg_display/main/ModbusClient.h
- firmware/lpg_display/main/ModbusClient.cpp
- firmware/lpg_display/main/screens/DashboardScreen.cpp
- firmware/lpg_display/main/screens/FillProgressScreen.cpp
- firmware/lpg_display/main/screens/FaultScreen.cpp
- firmware/lpg_display/sdkconfig.defaults
- firmware/lpg_controller/include/SettingsStore.h
- firmware/lpg_controller/src/SettingsStore.cpp
- firmware/lpg_controller/include/NetworkManager.h
- firmware/lpg_controller/src/NetworkManager.cpp
- firmware/lpg_controller/include/MqttService.h
- firmware/lpg_controller/src/MqttService.cpp
- firmware/lpg_controller/src/WebPortal.cpp
- firmware/lpg_controller/include/ModbusRegisterMap.h
- firmware/lpg_controller/src/ModbusRegisterMap.cpp
- firmware/lpg_controller/include/InputTruthTable.h
- firmware/lpg_controller/src/FillController.cpp
- firmware/lpg_controller/include/RtcService.h
- firmware/lpg_controller/src/RtcService.cpp

Do not duplicate completed V2 work:
- alarmCode/alarmSeverity/readinessMask/blockerMask already exist.
- diagnostic registers 0x0048..0x0050 already exist.
- RTC Unix timestamp registers 0x0026/0x0027 already exist and must not be overwritten.

Phase 1: finish professional behavior.
1. In display ModbusClient, increase RTU timeout to 300 ms or read from settings.
2. Add retry wrappers for read/write.
3. Add CommHealth enum: ONLINE, UNSTABLE, OFFLINE.
4. Do not show hard offline after one missed RTU frame.
5. Fix startFill() so it writes parameters with retries, writes command with retries, then reads state register 0x000E to confirm Validating/FillingFast/FillingSlow before declaring failure.
6. Fix FaultScreen to show exact alarmCode/blockerMask reason, not generic System fault detected.
7. Fix FillProgressScreen flicker using set-if-changed label updates and only update progress bar when value changes.
8. Show date/time or RTC NOT SET on Dashboard, Fault, and FillProgress screens. RTC validity must be based on date range, not hour/minute.
9. Review sdkconfig.defaults for ESP32-S3 USB Serial/JTAG console. Do not break bench-tested USB CDC if intentionally used.

Phase 2: minimum on-site deployment.
1. Add DisplaySettings.h/cpp with persistent RTU settings: slaveAddress, baudRate, parity, stopBits, timeoutMs=300, retries=2, unstableDebounceMs=800, offlineDebounceMs=3000.
2. Replace hardcoded kRtuBaud/kRtuAddr use in ModbusClient with DisplaySettings.
3. Add minimal display Admin/Controller Link screen for status, saved RTU settings, save/reconnect, and test link.
4. Add controller static IP fields to SettingsStore and apply WiFi.config() before WiFi.begin() when DHCP is false.
5. Add identity fields: stationId, controllerId, siteName, nozzleId.
6. Add POST /api/mqtt/test.
7. Add stationId/controllerId/timestamp/alarmCode/alarmSeverity/readinessMask/blockerMask to MQTT payload.
8. Use topic format <topicPrefix>/<stationId>/<controllerId>/status etc.
9. Add GET /api/config/export and POST /api/config/import skeleton. Do not export WiFi or MQTT passwords by default.
10. Add GET/POST /api/commissioning with commissioningComplete and blockers.

Phase 3, only after Phase 1 and Phase 2 build cleanly:
1. Add HX711 calibration wizard improvements.
2. Add GET /api/io/live and POST /api/io/mapping.
3. Update FillController::syncInputs() to use verified configurable input mapping.
4. Add POST /api/relay/test-pulse with safety locks.
5. Add RTC valid/lost-power Modbus flags only after current map end, never at 0x0026/0x0027.

Docs:
Update only these first:
- firmware/lpg_controller/README.md
- firmware/lpg_display/README.md
- DEPLOYMENT.md
- docs/modbus_and_realtime_protocol.md
Add:
- docs/COMMISSIONING_GUIDE.md
- docs/MODBUS_RTU_CURRENT_MAP.md
- docs/MQTT_INTEGRATION.md

Docs must remove or mark as legacy:
- old 0x1001 active map
- old path firmware/kc868_a6_lpg_controller
- statement that RTU is not enabled if RTU is active
- production-looking default WiFi credentials like Rao/password123

Constraints:
- Do not make ESP32 an MQTT broker.
- MQTT remains client-only and optional.
- MQTT remote start/stop remains disabled by default.
- Local filling must work if WiFi/MQTT are offline.
- Do not hard-change input mapping until bench verification or configurable mapping is implemented.
- Build display and controller after each phase.
```

---

## 11. Final Priority Summary

Do these first:

1. **Display Modbus stability** — timeout, retries, `UNSTABLE`, start confirmation.
2. **Fault/progress UI quality** — exact alarm reason, RTC valid/date, no flicker.
3. **Display RTU settings** — persistent address/baud/timeout/retries.
4. **Controller static IP + identity** — on-site network setup without code changes.
5. **MQTT test + proper payload** — broker config and commissioning confidence.
6. **Config export/import + commissioning summary** — technician handover and backup.
7. **HX711/input/relay/RTC wizards** — complete on-site commissioning.
8. **Docs cleanup** — remove legacy paths/maps/passwords and document active behavior.
