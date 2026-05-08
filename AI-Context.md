# AI Context - LPG Filling ESP

This file is the entry point for future coding agents. It replaces older chat-history knowledge. Read it before editing firmware.

## Current Firmware State

The active firmware is split into two projects:

- Controller firmware: `firmware/lpg_controller`
- Display firmware: `firmware/lpg_display`

Do not use or recreate the old `firmware/kc868_a6_lpg_controller` path. It is stale.

The current handoff is documented in:

- `docs/FIRMWARE_HANDOFF.md`
- `docs/FIRMWARE_BENCH_VERIFICATION_LOG.md`
- `docs/MODBUS_RTU_CURRENT_MAP.md`
- `docs/modbus_and_realtime_protocol.md`
- `docs/BOARD_REFERENCE.md`

## Important Truths

- Active Modbus RTU addresses are 0-based holding register addresses from `firmware/lpg_controller/include/ModbusRegisterMap.h`.
- Device ID register is holding register `0x0018`, value `0xA601`.
- The old `0x1001..0x1006` map is legacy only. Do not use it for display firmware or new integrations.
- Controller production builds disable Modbus writes by default.
- Prototype builds enable Modbus writes with `-DLPG_PROTOTYPE_BUILD=1` or `-DLPG_MODBUS_WRITES_ENABLED=1`.
- Production controller firmware must not contain hardcoded STA credentials. The old `Rao` / `password123` defaults are dev-only behind `-DLPG_DEV_WIFI_DEFAULTS=1`.
- Display USB console is USB Serial/JTAG, not USB CDC.
- Current bench ports:
  - Controller: `COM10`
  - Display: `COM8`

## Verified On Bench

As of 2026-05-08:

- Controller on `COM10` reports RTU enabled, slave `1`, `9600 8N1`, RX `14`, TX `27`.
- Controller readiness reported OK: nozzle `1`, cylinder `1`, estop OK `1`.
- Display on `COM8` booted latest firmware and RTU health reached `ONLINE`.
- Display read Device ID register `0x0018` and received `0xA601`.

See `docs/FIRMWARE_BENCH_VERIFICATION_LOG.md` for the exact serial evidence.

## Build Commands

Controller production build:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_firmware.ps1
```

Controller prototype build:

```powershell
.\tools\local\arduino-cli.exe compile --fqbn esp32:esp32:esp32 --build-property "compiler.cpp.extra_flags=-DLPG_PROTOTYPE_BUILD=1 -DLPG_DEV_WIFI_DEFAULTS=1" firmware\lpg_controller
```

Display build:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
$env:IDF_PATH='C:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\python_env\idf5.5_py3.11_env'
. "$env:IDF_PATH\export.ps1"
python "$env:IDF_PATH\tools\idf.py" -C firmware/lpg_display build
```

## Rules Future Agents Must Not Break

- Do not change firmware behavior when asked for documentation only.
- Do not use the old `0x1001` Modbus map for display work.
- Do not enable production Modbus writes by default.
- Do not restore hardcoded production WiFi credentials.
- Do not change known-good RS485 pins without reading `docs/BOARD_REFERENCE.md`.
- Do not flash the display with the wrong board target or stale ESP-IDF config.
- Do not leave relays energized on fault, abort, stop, or reset paths.
- Do not overwrite user-generated bench images or untracked files unless explicitly asked.

## Remaining Hardware Tests

These still require deliberate bench actions:

- Force one missed RTU frame and confirm display shows `UNSTABLE`, not `OFFLINE`.
- Interrupt controller/RS485 long enough to confirm `OFFLINE`.
- Press Start Fill and confirm target/rate/amount/command writes plus confirm-state behavior in live logs.
- Trigger a real fault and confirm `FaultScreen` shows the real alarm reason.

## Change Log

### 2026-05-08 - Codex - Firmware Handoff Refresh

Replaced stale SOP content with current firmware handoff pointers. Marked old controller path, old `0x1001` Modbus map, and old production WiFi defaults as stale. Added bench verification references for COM10 controller and COM8 display.
