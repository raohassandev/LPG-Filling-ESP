# LPG Filling ESP — Codex Quick Fix Plan v2

**Branch:** `codex/firmware-ui-history`
**Purpose:** Apply small, accurate fixes without re-reading/refactoring the full repo.
**Instruction to Codex:** Do not rewrite architecture. Do not rename folders. Do not change hardware pin/input mapping without confirming against the current runtime wiring and bench test. Make targeted changes only.

---

## 0. Current Understanding Check

Codex's understanding is mostly correct, but update the previous plan with these corrections:

### Already added / do not duplicate
The current project already includes:

- `alarmCode`
- `alarmSeverity`
- `readinessMask`
- `blockerMask`
- Modbus diagnostic registers `0x0048..0x0050`
- dashboard date/time display
- dashboard alarm text
- docs for new diagnostic registers

Do not add another duplicate alarm/diagnostics block.

### Critical correction
Do **not** use Modbus holding registers `0x0026` and `0x0027` for RTC validity or RTC lost-power status.

These registers are already used for:

```cpp
kHR_RtcUnixHi = 0x0026; // UINT32 Hi — Unix timestamp
kHR_RtcUnixLo = 0x0027; // UINT32 Lo — write Lo triggers RTC set
```

If RTC status must be exposed over Modbus, add a new register **after the current map end**, for example:

```cpp
constexpr uint16_t kHR_RtcStatus = 0x0051;
// bit0 = rtcInitialized
// bit1 = rtcLostPower
// bit2 = rtcDateValid
// bit3 = rtcReadOk

constexpr uint16_t kHR_Count = 0x0052; // because map now ends at 0x0051
```

However, the fastest safe fix is to validate RTC locally in the display from existing `rtcYear/month/day/hour/minute/second`.

---

## 1. Real Issues to Fix Now

### Issue A — False “Controller Offline / Check RS485” popup during Start Fill

Observed behavior:

- User presses **START FILL**.
- Sometimes popup says **Controller Offline, Check RS485**.
- On second or third attempt, filling starts.
- This is not professional behavior.

Root cause:

`ModbusClient::startFill()` performs several sequential RTU writes:

1. target weight
2. rate
3. amount
4. start command

If any one write response is missed, `startFill()` returns `false`, even if the controller is still alive or even if the command was accepted but the response was lost.

### Required behavior

A single missed RTU frame must not be treated as hard offline.

Implement 3 communication states:

```cpp
enum class CommHealth : uint8_t {
  Online = 0,
  Unstable = 1,
  Offline = 2,
};
```

Professional UI behavior:

| Condition | Display behavior |
|---|---|
| 1 failed Modbus request | Keep last data, no modal popup |
| 2-3 intermittent failures | Small top status: `RS485 unstable` |
| No successful poll for >3-5 sec | Show `Controller offline` |
| Start command write failed but controller state changed to filling | Treat start as success |
| Start command could not be confirmed | Show `Command not confirmed`, not `Controller offline` |

---

## 2. Modbus Timeout and Retry Fix

### File
`firmware/lpg_display/main/ModbusClient.h`

### Change timeout

Current timeout is too tight for 9600 RTU and screen-side task scheduling.

Change:

```cpp
static constexpr int kTimeoutMs = 150;
```

To:

```cpp
static constexpr int kTimeoutMs = 300;
static constexpr uint8_t kReadAttempts = 2;
static constexpr uint8_t kWriteAttempts = 3;
static constexpr int64_t kOfflineUs = 5000000;   // 5 sec
static constexpr int64_t kUnstableUs = 1500000;  // 1.5 sec
```

### Add fields to `ControllerSnapshot`

```cpp
CommHealth commHealth = CommHealth::Offline;
uint16_t commFailStreak = 0;
uint16_t commOkStreak = 0;
int64_t lastFailUs = 0;
```

### Add helper declarations to `ModbusClient`

```cpp
bool readHRRetry(uint16_t start, uint16_t count, uint16_t* out, uint8_t attempts = kReadAttempts);
bool writeRegisterRetry(uint16_t reg, uint16_t val, uint8_t attempts = kWriteAttempts);
bool writeRegistersRetry(uint16_t startReg, const uint16_t* values, uint16_t count, uint8_t attempts = kWriteAttempts);

void noteCommOk();
void noteCommFail();
void updateCommHealth();
bool confirmFillStarted(uint32_t waitMs = 900);
```

---

## 3. Modbus Health State Implementation

### File
`firmware/lpg_display/main/ModbusClient.cpp`

Add these helpers:

```cpp
void ModbusClient::noteCommOk() {
  const int64_t now = now_us();
  snap_.commOkStreak++;
  snap_.commFailStreak = 0;
  snap_.connected = true;
  snap_.valid = true;
  snap_.lastOkUs = now;
  updateCommHealth();
}

void ModbusClient::noteCommFail() {
  const int64_t now = now_us();
  snap_.commFailStreak++;
  snap_.lastFailUs = now;
  updateCommHealth();
}

void ModbusClient::updateCommHealth() {
  const int64_t now = now_us();

  if (snap_.lastOkUs == 0 || (now - snap_.lastOkUs) > kOfflineUs) {
    snap_.connected = false;
    snap_.commHealth = CommHealth::Offline;
    return;
  }

  snap_.connected = true;

  if (snap_.commFailStreak >= 2 || (now - snap_.lastFailUs) < kUnstableUs) {
    snap_.commHealth = CommHealth::Unstable;
    return;
  }

  snap_.commHealth = CommHealth::Online;
}
```

Add retry helpers:

```cpp
bool ModbusClient::readHRRetry(uint16_t start, uint16_t count, uint16_t* out, uint8_t attempts) {
  for (uint8_t i = 0; i < attempts; ++i) {
    if (readHR(start, count, out)) {
      noteCommOk();
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(40 + i * 40));
  }
  noteCommFail();
  return false;
}

bool ModbusClient::writeRegisterRetry(uint16_t reg, uint16_t val, uint8_t attempts) {
  for (uint8_t i = 0; i < attempts; ++i) {
    if (writeRegister(reg, val)) {
      noteCommOk();
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(50 + i * 50));
  }
  noteCommFail();
  return false;
}

bool ModbusClient::writeRegistersRetry(uint16_t startReg, const uint16_t* values, uint16_t count, uint8_t attempts) {
  for (uint8_t i = 0; i < attempts; ++i) {
    if (writeRegisters(startReg, values, count)) {
      noteCommOk();
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(50 + i * 50));
  }
  noteCommFail();
  return false;
}
```

Then replace normal polling calls:

```cpp
if (readHR(0x0000, 24, r)) {
```

With:

```cpp
if (readHRRetry(0x0000, 24, r)) {
```

Also update RTC, diagnostics, and stats reads to use `readHRRetry()`.

Do not set `connected=false` directly after one failed read. Let `updateCommHealth()` handle it.

---

## 4. Start Fill Confirmation Fix

### File
`firmware/lpg_display/main/ModbusClient.cpp`

Add helper:

```cpp
bool ModbusClient::confirmFillStarted(uint32_t waitMs) {
  const int loops = waitMs / 150;

  for (int i = 0; i < loops; ++i) {
    uint16_t stateReg = 0;
    if (readHRRetry(0x000E, 1, &stateReg, 1)) {
      const uint16_t s = stateReg;

      // Validating=2, Fast=3, Slow=4, Settling=5, Complete=6
      if (s >= 2 && s <= 6) {
        snap_.state = static_cast<FillState>(s);
        return true;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(150));
  }

  return false;
}
```

Modify `startFill()` sequence:

```cpp
if (!writeRegistersRetry(0x0006, targetRegs, 2)) {
  ESP_LOGW(TAG, "Start fill failed: target write not confirmed");
  unlock();
  return false;
}

vTaskDelay(pdMS_TO_TICKS(80));

if (!writeRegistersRetry(0x0008, rateRegs, 2)) {
  ESP_LOGW(TAG, "Start fill failed: rate write not confirmed");
  unlock();
  return false;
}

vTaskDelay(pdMS_TO_TICKS(80));

if (!writeRegistersRetry(0x000A, amountRegs, 2)) {
  ESP_LOGW(TAG, "Start fill failed: amount write not confirmed");
  unlock();
  return false;
}

vTaskDelay(pdMS_TO_TICKS(80));

if (!writeRegisterRetry(0x0017, 1)) {
  ESP_LOGW(TAG, "Start command response missing; checking controller state");

  if (confirmFillStarted(900)) {
    ESP_LOGW(TAG, "Start command accepted by controller despite missing RTU response");
    unlock();
    return true;
  }

  unlock();
  return false;
}

if (!confirmFillStarted(900)) {
  ESP_LOGW(TAG, "Start command written but fill state not confirmed");
  // Do not mark controller offline here.
  // Let UI show "Command not confirmed" or "Waiting for controller".
  unlock();
  return false;
}

ESP_LOGI(TAG, "Start fill confirmed: target=%.3fkg rate=%.2f amount=%.2f",
         targetWeightKg, ratePerKg, targetAmount);

unlock();
return true;
```

Important:

- Do not show offline popup just because `startFill()` returns false.
- UI must inspect `snapshot().commHealth`.
- Only show **Controller Offline** when `commHealth == Offline`.

---

## 5. UI Popup Policy

### File candidates

Search:

```bash
grep -R "Controller Offline\|Check RS485\|Offline" -n firmware/lpg_display/main
```

Replace any start-button failure popup logic with this policy:

```cpp
const auto& snap = modbus.snapshot();

if (snap.commHealth == CommHealth::Offline) {
  showToast("Controller offline. Check RS485 wiring, power, and slave address.", ToastLevel::Alarm);
} else if (snap.commHealth == CommHealth::Unstable) {
  showToast("RS485 unstable. Command not confirmed. Please retry.", ToastLevel::Warning);
} else {
  showToast("Start rejected. Check readiness/fault reason.", ToastLevel::Warning);
}
```

Do not use a large blocking modal for one transient communication failure.

---

## 6. RTC / Date-Time Fix

### Problem

The display may show only `00:00`, or show time without a reliable date. RTC validity should not be based on "time is non-zero". Midnight is valid. Date range must be checked.

### File
`firmware/lpg_display/main/DisplayFormat.h` or whichever file formats header date/time.

Add:

```cpp
static inline bool isValidRtcDateTime(const ControllerSnapshot& s) {
  if (s.rtcYear < 2024 || s.rtcYear > 2099) return false;
  if (s.rtcMonth < 1 || s.rtcMonth > 12) return false;
  if (s.rtcDay < 1 || s.rtcDay > 31) return false;
  if (s.rtcHour > 23 || s.rtcMinute > 59 || s.rtcSecond > 59) return false;
  return true;
}
```

Header display rule:

```cpp
char dt[32];

if (isValidRtcDateTime(snap)) {
  snprintf(dt, sizeof(dt), "%04u-%02u-%02u  %02u:%02u",
           snap.rtcYear, snap.rtcMonth, snap.rtcDay,
           snap.rtcHour, snap.rtcMinute);
} else {
  snprintf(dt, sizeof(dt), "RTC NOT SET");
}
```

Do this consistently on:

- Dashboard/header
- Fill progress screen/header
- Fault screen/header
- Settings/diagnostics screen if present

### Optional Modbus register for RTC status

Only if needed, add this at the end of the current map:

```cpp
constexpr uint16_t kHR_RtcStatus = 0x0051;
// bit0 = rtcInitialized
// bit1 = rtcLostPower
// bit2 = rtcDateValid
// bit3 = rtcReadOk
constexpr uint16_t kHR_Count = 0x0052;
```

Do **not** overwrite `0x0026/0x0027`.

---

## 7. Fault Screen Exact Reason

### Problem

Fault screen must not show only:

```text
System fault detected
Check nozzle connection, cylinder placement, and e-stop status...
```

The controller already exposes `alarmCode`, `alarmSeverity`, `readinessMask`, and `blockerMask`.

### Required mapping

Add one central mapping, preferably in `DisplayFormat.h` or a new small `AlarmText.h`:

```cpp
static inline const char* alarmTitle(uint16_t code) {
  switch (code) {
    case 0: return "No alarm";
    case 1: return "Emergency stop active";
    case 2: return "Nozzle not engaged";
    case 3: return "Cylinder not detected";
    case 4: return "Scale read error";
    case 5: return "Scale not stable";
    case 6: return "Scale not calibrated";
    case 7: return "Overfill alarm";
    case 8: return "Fill timeout";
    case 9: return "No flow detected";
    case 10: return "Transaction log fault";
    case 11: return "Stopped by operator";
    case 12: return "Controller fault active";
    case 13: return "Controller offline";
    default: return "Unknown alarm";
  }
}

static inline const char* alarmHint(uint16_t code) {
  switch (code) {
    case 1: return "Release E-stop, then press RESET.";
    case 2: return "Check nozzle switch and wiring.";
    case 3: return "Place cylinder or check cylinder input.";
    case 4: return "Check HX711/load cell wiring and power.";
    case 5: return "Wait for stable weight before starting.";
    case 6: return "Calibrate scale before filling.";
    case 7: return "Close valve and check target/scale.";
    case 8: return "Check valve, gas flow, and timeout setting.";
    case 9: return "Weight is not increasing. Check gas flow, relay, valve, or simulation.";
    case 10: return "Check SD card/log storage.";
    case 11: return "Press RESET to return to ready state.";
    case 13: return "Check RS485 A/B wiring, GND, slave address, and controller power.";
    default: return "Check readiness details, then reset.";
  }
}
```

Use this on:

- Dashboard alarm area
- Fault screen
- Fill progress screen bottom/status area

---

## 8. Screen Flicker Fix

### Problem

Screen flicker appears when Modbus values change. Likely causes:

- Repeated `lv_label_set_text()` even when text is same
- Repeated style changes even when state is same
- Updating high-frequency values without threshold
- Possibly recreating UI elements on state refresh

### Required helper

Create `firmware/lpg_display/main/UiHelpers.h`:

```cpp
#pragma once

#include "lvgl.h"
#include <string.h>

static inline void setLabelTextIfChanged(lv_obj_t* label, const char* text) {
  if (!label || !text) return;
  const char* old = lv_label_get_text(label);
  if (!old || strcmp(old, text) != 0) {
    lv_label_set_text(label, text);
  }
}

static inline void setObjHiddenIfChanged(lv_obj_t* obj, bool hidden) {
  if (!obj) return;
  const bool nowHidden = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
  if (nowHidden != hidden) {
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}
```

Use this in:

- `DashboardScreen.cpp`
- `FillProgressScreen.cpp`
- `FaultScreen.cpp`
- `SettingsScreen.cpp`

### Numeric update thresholds

Do not refresh weight text on every tiny Modbus change.

Use:

```cpp
static bool changedBy(float a, float b, float eps) {
  return fabsf(a - b) >= eps;
}
```

Recommended thresholds:

| Value | Threshold |
|---|---:|
| live weight | `0.005 kg` |
| net weight | `0.005 kg` |
| amount | `0.50 PKR` |
| progress percent | `0.1%` |
| time/date | once per second or when minute changes in header |
| state/alarm labels | only on value change |

Also avoid large screen switches unless state changes and remains stable for at least one poll cycle.

---

## 9. Input Truth Table Alignment

### Current conflict

`InputTruthTable.h` documents:

```text
Input 0 = nozzle
Input 1 = cylinder
Input 2 = emergency stop
Input 3 = door interlock
```

But Codex observed runtime logic appears to use:

```text
Input 0 = cylinder
Input 1 = nozzle
Input 3 = E-stop tripped
```

### Do not blindly change this

This can create a dangerous safety bug. The correct fix is:

1. Find current `FillController::syncInputs()`.
2. Find physical KC868-A6 wiring actually used in the prototype.
3. Make runtime and docs use one single source of truth.

### Fastest safe code direction

Move input mapping to `BoardConfig.h`:

```cpp
static constexpr uint8_t kInputCylinderPresent = 0;
static constexpr uint8_t kInputNozzleEngaged = 1;
static constexpr uint8_t kInputEmergencyStop = 3;

// true means raw input is ON when E-stop is pressed/tripped.
// false means raw input is ON when E-stop loop is healthy.
static constexpr bool kInputEmergencyRawMeansTripped = true;
```

Then in `FillController::syncInputs()`:

```cpp
const bool rawCylinder = inputExpander_.inputState(kInputCylinderPresent);
const bool rawNozzle = inputExpander_.inputState(kInputNozzleEngaged);
const bool rawEStop = inputExpander_.inputState(kInputEmergencyStop);

const bool emergencyOk = kInputEmergencyRawMeansTripped ? !rawEStop : rawEStop;

statusStore_.setCylinderPresent(rawCylinder);
statusStore_.setNozzleEngaged(rawNozzle);
statusStore_.setEmergencyStopOk(emergencyOk);
```

Then update `InputTruthTable.h` comments to match these constants.

### Bench test acceptance

Serial monitor must print a raw input diagnostic table:

```text
IN0=...
IN1=...
IN2=...
IN3=...
cylinderPresent=...
nozzleEngaged=...
emergencyStopOk=...
```

Acceptance:

- Remove cylinder sensor → UI cylinder icon red and start blocked by cylinder reason.
- Remove nozzle → UI nozzle icon red and start blocked by nozzle reason.
- Press E-stop → UI E-stop red and controller prevents valve outputs.
- Release E-stop + reset → system can return to ready.

---

## 10. UI/UX Polish Requirements

The UI should look like a filling station product, not a debug demo.

### Header

Every screen header should show:

```text
LPG FILLING STATION        YYYY-MM-DD HH:MM        RS485: OK/UNSTABLE/OFFLINE
```

Use `RTC NOT SET` when date invalid.

### Dashboard

- Use stable colors.
- Show `READY` only if no blocker bits are active.
- If ready false, show first blocker reason.
- Do not say "System ready" while hidden blockers exist.
- Avoid tiny text in important cards.
- Keep START button disabled until true readiness.

### Fill progress screen

Current screen is too empty. Add:

- State badge: `FAST FILL`, `SLOW FILL`, `SETTLING`, `COMPLETE`
- Date/time in header
- RS485 health in header
- Actual kg
- Target kg
- Amount current / target
- Rate PKR/kg
- Alarm/status line at bottom
- Stop fill button

### Fault screen

Show:

```text
FAULT
<alarmTitle(alarmCode)>
<alarmHint(alarmCode)>
Code: <alarmCode>  Blocker: 0x....
[RESET]
```

### Communication messages

Do not show large blocking popup for a single dropped Modbus frame. Use small non-blocking toast/status area unless `commHealth == Offline`.

---

## 11. Documentation Cleanup

### Required grep

Run:

```bash
grep -R "0x1001\|kc868_a6_lpg_controller\|RTU is not enabled\|0x0026\|0x0027" -n README.md docs firmware apps
```

### Documentation rules

Update docs so they clearly say:

- Active firmware folders:
  - `firmware/lpg_controller`
  - `firmware/lpg_display`
- Active display communication:
  - Modbus RTU
  - 9600 baud unless changed in config
  - 0-based PDU addresses
- Active alarm diagnostics:
  - `0x0048 alarmCode`
  - `0x0049 alarmSeverity`
  - `0x004A readinessMask`
  - `0x004B blockerMask`
  - `0x004C scaleInitialized`
  - `0x004D scaleReadError`
  - `0x004E calibrationValid`
  - `0x004F simulationActive`
  - `0x0050 alarmSource`
- RTC:
  - `0x0020..0x0025` date/time
  - `0x0026..0x0027` Unix timestamp
  - Do not document `0x0026/0x0027` as RTC validity.

If old TCP/prototype maps are kept, move them under an `archive/legacy` section and clearly mark:

```text
Legacy reference only. Not used by current display firmware.
```

---

## 12. Acceptance Tests

### A. RTU resilience

Test by pressing Start Fill 20 times under normal wiring.

Pass criteria:

- No false "Controller Offline" popup while normal polling is working.
- If one RTU frame is missed, UI shows at most `RS485 unstable`.
- If command response is missed but state becomes Fast/Slow, UI enters progress screen.

### B. Real offline test

Disconnect RS485 A/B.

Pass criteria:

- UI does not instantly throw modal on first failed request.
- Within about 5 seconds, header says `RS485 OFFLINE`.
- Start button disabled or start action shows proper offline message.

### C. RTC

Set RTC to valid date.

Pass criteria:

- All screens show date and time.
- `00:00` at midnight is accepted as valid.
- If date is invalid, UI shows `RTC NOT SET`.

### D. Fault reason

Create each fault condition.

Pass criteria:

- Fault screen shows exact reason, not generic `System fault detected`.
- Dashboard and progress screen show same alarm title.

### E. Flicker

Observe screen for 2 minutes during live Modbus updates.

Pass criteria:

- No visible flicker during normal numeric updates.
- Labels do not redraw unless changed.
- Screen transition happens only on stable state change.

### F. Input truth table

Toggle each input physically.

Pass criteria:

- Raw input diagnostics match physical wiring.
- UI icons match physical input.
- Start blockers match correct input names.

---

## 13. Exact Codex Prompt

Copy this to Codex:

```text
You are fixing the LPG-Filling-ESP repo on branch codex/firmware-ui-history.

Do targeted fixes only. Do not refactor the whole project.

Main goals:
1. Fix false "Controller Offline / Check RS485" popup during Start Fill.
2. Add Modbus comm health states: Online, Unstable, Offline.
3. Increase RTU timeout from 150 ms to 300 ms.
4. Add retry wrappers for RTU read/write.
5. Modify startFill so missed RTU response does not equal failure if controller state confirms fill started.
6. UI must only show Controller Offline when commHealth == Offline. Single dropped frame should show no modal; intermittent failures show RS485 unstable.
7. Fix RTC display by validating date range, not time nonzero. Midnight 00:00 is valid.
8. Do not overwrite Modbus registers 0x0026/0x0027. They are Unix timestamp Hi/Lo. If adding RTC status, use a new register after current map end, e.g. 0x0051.
9. Apply alarmCode/alarmHint to FaultScreen and FillProgressScreen, not only dashboard.
10. Reduce LVGL flicker using set-if-changed helpers and numeric thresholds.
11. Align InputTruthTable.h with runtime wiring. Do not blindly change input mapping; move mapping constants to BoardConfig.h and update docs after bench test.
12. Update docs so active RTU map and current folders are correct. Mark old 0x1001/TCP docs as legacy if retained.

Acceptance:
- 20 start attempts should not show false offline while controller is actually polling.
- Disconnect RS485: offline appears only after debounce/timeout.
- RTC valid date/time shown on all screens; invalid date shows RTC NOT SET.
- Fault screen shows exact alarm reason and hint.
- No visible flicker on normal Modbus updates.
- Input diagnostic serial output confirms real wiring.
```

---

## 14. Do Not Do These

- Do not add duplicate alarm registers.
- Do not overwrite `0x0026/0x0027`.
- Do not display "Controller Offline" for a single failed start write.
- Do not change E-stop input mapping without bench verification.
- Do not recreate LVGL objects during periodic updates.
- Do not keep docs saying RTU is not enabled if current display uses RTU.
- Do not do broad UI redesign before communication stability is fixed.
