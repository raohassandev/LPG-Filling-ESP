# Codex Next TODO After V2

**Repo/branch:** `codex/firmware-ui-history`
**Created from:** `Audit/CODEX_LOW_RESOURCE_TODO_AFTER_V2_AND_ONSITE.md`
**Rule:** Do not repeat completed V2 fixes. Verify, commit, then continue with small Phase 2 deployment changes.

---

## 0. Current Working Tree Status

The current uncommitted work already appears to include:

- Display RTU timeout increased to 300 ms.
- Display RTU retry wrappers.
- Display communication health: online / unstable / offline.
- Start Fill confirmation after Modbus writes.
- UI policy avoiding false hard-offline popup on one missed RTU frame.
- Shared display helpers in `firmware/lpg_display/main/UiHelpers.h`.
- Fault/progress/complete screens using improved alarm and RTC formatting.
- Controller input mapping constants moved to `BoardConfig.h`.
- Controller `/api/io/live` endpoint.
- Controller serial `io` / `inputs` diagnostic command for raw input verification without Expo token.
- Docs partially updated for active Modbus diagnostics and input mapping.

Do not duplicate these.

---

## 1. Immediate Next Step: Verify And Commit V2

### 1.1 Hardware verification

Use controller serial on `COM10`:

```text
io
status
```

Acceptance:

- Cylinder removed -> raw input changes, interpreted `cylinder=0`, display blocks start.
- Nozzle unlocked -> raw input changes, interpreted `nozzle=0`, display blocks start.
- E-stop pressed -> raw Input 4 active/tripped, interpreted `estopOk=0`, valves blocked.
- E-stop released + reset -> interpreted `estopOk=1`, system returns ready.
- Display Start Fill does not show false `Controller offline` while live Modbus polling is working.

### 1.2 Build verification

Run before commit:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_firmware.ps1
```

For display:

```powershell
$env:IDF_PATH='C:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\python_env\idf5.5_py3.11_env'
. "$env:IDF_PATH\export.ps1"
python "$env:IDF_PATH\tools\idf.py" -C firmware/lpg_display build
```

### 1.3 Commit scope

Commit only the V2 implementation files and matching docs. Be careful with:

- `Audit/LPG-Filling-ESP-Deep-Audit.md` is deleted in the working tree. Do not stage this deletion unless intentionally removing it.
- Audit plan files are untracked. Stage only if they are meant to become project artifacts.

Suggested commit:

```text
fix: stabilize display modbus and expose input diagnostics
```

---

## 2. Phase 2A: Display Persistent RS485 Settings

Goal: allow onsite technician to change display-to-controller RTU settings without source edits.

**Status:** Implemented in firmware and display build passed.

Files:

```text
firmware/lpg_display/main/DisplaySettings.h
firmware/lpg_display/main/DisplaySettings.cpp
firmware/lpg_display/main/DisplayConfig.h
firmware/lpg_display/main/ModbusClient.h
firmware/lpg_display/main/ModbusClient.cpp
```

Implement:

- NVS-backed display settings.
- `DisplayRtuSettings`:
  - `slaveAddress = 1`
  - `baudRate = 9600`
  - `parity = 0`
  - `stopBits = 1`
  - `timeoutMs = 300`
  - `retries = 2`
  - `unstableDebounceMs = 800`
  - `offlineDebounceMs = 3000`
- `DisplayIdentitySettings`:
  - `displayId = DSP-001`
  - `stationId = LPG-STN-001`
- `ModbusClient::begin(const DisplayRtuSettings&)`
- `ModbusClient::applySettings(const DisplayRtuSettings&)`

Keep UART pins in `DisplayConfig.h`. Only make address, baud, parity, stop bits, timeout, retries, and debounces configurable.

Acceptance:

- Defaults match current working RTU link.
- Reboot keeps saved settings.
- Bad settings can be corrected without reflashing if UI/settings screen is present.

---

## 3. Phase 2B: Minimal Controller Link Screen On Display

Goal: make RS485 status visible and serviceable from display UI.

**Status:** Pending. ModbusClient now supports saved RTU settings and runtime apply/reconnect, but the Admin UI screen is not built yet.

Minimum UI:

```text
Admin -> Controller Link
Status: ONLINE / UNSTABLE / OFFLINE
Slave Address
Baud Rate
Parity
Stop Bits
Timeout ms
Retries
Save & Reconnect
Test Link
```

Use existing display screen/navigation patterns. Do not redesign the whole UI.

Acceptance:

- Technician can view current RS485 health.
- Technician can test link.
- Technician can save RTU settings and reconnect.

---

## 4. Phase 2C: Controller Static IP And Identity

Goal: configure onsite networking and identity without firmware edits.

**Status:** Implemented in firmware and controller build passed.

Files:

```text
firmware/lpg_controller/include/SettingsStore.h
firmware/lpg_controller/src/SettingsStore.cpp
firmware/lpg_controller/include/NetworkManager.h
firmware/lpg_controller/src/NetworkManager.cpp
firmware/lpg_controller/src/WebPortal.cpp
```

Add:

- Static IP settings:
  - `dhcp`
  - `ip`
  - `gateway`
  - `subnet`
  - `dns1`
  - `dns2`
- Device identity:
  - `stationId`
  - `controllerId`
  - `siteName`
  - `nozzleId`

Expose in existing network/system APIs where possible. Avoid a new settings store unless `SettingsStore` becomes unmanageable.

Acceptance:

- DHCP remains default.
- Static IP applies before `WiFi.begin()`.
- Invalid static IP safely falls back to DHCP.
- Identity appears in system/commissioning responses.

---

## 5. Phase 2D: MQTT Test And Deployment Payload

Goal: verify broker configuration and publish useful deployment diagnostics.

**Status:** Implemented in firmware and controller build passed. MQTT remains optional/client-only and remote start/stop was not added.

Files:

```text
firmware/lpg_controller/include/MqttService.h
firmware/lpg_controller/src/MqttService.cpp
firmware/lpg_controller/src/WebPortal.cpp
```

Implement:

- `POST /api/mqtt/test`
- MQTT status payload includes:
  - `deviceType`
  - `stationId`
  - `controllerId`
  - `ts`
  - state, live/net/target kg, amount
  - readiness booleans
  - `alarmCode`
  - `alarmSeverity`
  - `readinessMask`
  - `blockerMask`
  - `uptimeSec`
- Topic format:

```text
<topicPrefix>/<stationId>/<controllerId>/status
<topicPrefix>/<stationId>/<controllerId>/transaction
<topicPrefix>/<stationId>/<controllerId>/alert
<topicPrefix>/<stationId>/<controllerId>/lwt
```

Keep MQTT optional and client-only. Do not enable remote start/stop by default.

---

## 6. Phase 2E: Config Export And Commissioning Summary

Goal: make onsite setup auditable and recoverable.

**Status:** Implemented as lightweight firmware endpoints and controller build passed. Import is intentionally a skeleton; export excludes secrets.

Files:

```text
firmware/lpg_controller/include/SettingsStore.h
firmware/lpg_controller/src/SettingsStore.cpp
firmware/lpg_controller/src/WebPortal.cpp
```

Add:

- `GET /api/config/export`
- `POST /api/config/import` skeleton
- `GET /api/commissioning`
- `POST /api/commissioning`

Export must not include WiFi or MQTT passwords by default.

Commissioning response should include:

- `commissioningComplete`
- `identityConfigured`
- `networkConfigured`
- `rtuConfigured`
- `scaleCalibrated`
- `inputsVerified`
- `mqttConfigured`
- `rtcValid`
- `productionLocked`
- `blockers`

---

## 7. Docs After Phase 2

Update only the useful docs first:

```text
firmware/lpg_controller/README.md
firmware/lpg_display/README.md
DEPLOYMENT.md
docs/modbus_and_realtime_protocol.md
docs/COMMISSIONING_GUIDE.md
docs/MODBUS_RTU_CURRENT_MAP.md
docs/MQTT_INTEGRATION.md
```

Must remove or mark legacy:

- old `0x1001` active map
- old `firmware/kc868_a6_lpg_controller` path
- statement that RTU is not enabled
- production-looking default credentials such as `Rao/password123`

**Status:** Implemented in docs. Added commissioning, active RTU map, MQTT integration, and display README docs. Corrected deployment/controller README paths and marked old `0x1001` map as legacy-only.

---

## 8. Later Phase 3

Do after Phase 2 builds cleanly:

- HX711 calibration wizard improvements.
- Configurable input mapping wizard.
- `POST /api/io/mapping`.
- Relay pulse test with safety locks.
- RTC validity/lost-power Modbus flags after current map end only.
