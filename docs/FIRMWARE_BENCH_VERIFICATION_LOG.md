# Firmware Bench Verification Log

Current date: 2026-05-08

This file records bench evidence and build evidence. Entries that pre-date a new firmware build remain historical hardware evidence only until the new binaries are flashed and tested.

## Bench Ports

| Device | Port | Firmware |
|---|---|---|
| LPG controller KC868-A6 | `COM10` | `firmware/lpg_controller` |
| ESP32-S3 display | `COM8` | `firmware/lpg_display` |

Earlier references to display `COM9` are stale for this bench session. The display currently enumerates on `COM8`.

## Controller Verification

Controller serial was opened on `COM10` at 115200 baud.

Commands run:

```text
rtu
status
io
```

Observed output:

```text
[SERIAL] rtu: enabled=1 slave=1 baud=9600 data=8 parity=0 stop=1 rx=14 tx=27 de=255
[STATUS] state=IDLE live=-0.059 tare=0.000 net=0.000 target=0.000 rate=250.00 amount=0.00 nozzle=1 cylinder=1 estop=1 reason=
[IO] rawInputs=1,1,0,0,0,0
[IO] mapping cylinder=IN1(active-high) nozzle=IN2(active-high) estop=IN4(raw-tripped=1)
[IO] interpreted cylinder=1 nozzle=1 estopOk=1
```

Verified:

- Controller is alive on `COM10`.
- Modbus RTU is enabled.
- RTU slave address is `1`.
- RTU serial is `9600 8N1`.
- KC868-A6 RS485 pins are RX `14`, TX `27`.
- RS485 DE is `255`, meaning no DE pin because the board uses auto-direction RS485.
- Readiness inputs were OK during this run: cylinder present, nozzle engaged, E-stop OK.

## Display Verification

Display serial was opened on `COM8` at 115200 baud.

The display was flashed with the current ESP-IDF build using direct `esptool`.

Relevant boot output:

```text
I (...) app_init: Project name:     lpg_display
I (...) app_init: App version:      a823453
I (...) MBUS: Modbus RTU UART1 TX=44 RX=43 slave=1 baud=9600 parity=0 stop=1 timeout=300 retries=2
```

After adding diagnostic RTU logs and reflashing, the display logged:

```text
I (...) MBUS: RTU health=ONLINE ok=1 fail=0 last_ok_us=452388 last_fail_us=0
I (...) MBUS: Device ID test: read 0xA601 (OK)
```

Verified:

- Display is alive on `COM8`.
- Display firmware boots with USB Serial/JTAG console.
- Display uses RTU UART1, TX `44`, RX `43`.
- Display RTU settings are slave `1`, `9600 8N1`.
- Display reached RTU `ONLINE`.
- Display read controller Device ID register `0x0018`.
- Controller returned Device ID `0xA601`.

## Device ID Test

Active Device ID register:

| Register | Value |
|---|---|
| `0x0018` | `0xA601` |

Display code path:

- `ModbusClient::readDeviceId()`
- `SettingsScreen::onTestLink()`
- Boot diagnostic log in `ModbusClient::poll()`

Expected success message:

```text
Device ID test: read 0xA601 (OK)
```

## Build Verification

Controller production build, after resource-register/dashboard-sync changes:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_firmware.ps1
```

Result:

```text
Sketch uses 1241678 bytes (94%) of program storage space.
Global variables use 55604 bytes (16%) of dynamic memory, leaving 272076 bytes.
```

Controller prototype build:

```powershell
.\tools\local\arduino-cli.exe compile --fqbn esp32:esp32:esp32 --build-property "compiler.cpp.extra_flags=-DLPG_PROTOTYPE_BUILD=1 -DLPG_DEV_WIFI_DEFAULTS=1" firmware\lpg_controller
```

Result:

```text
Sketch uses 1243453 bytes (94%) of program storage space.
Global variables use 55736 bytes (17%) of dynamic memory, leaving 271944 bytes.
```

Display build, after dashboard-sync changes:

```powershell
python "$env:IDF_PATH\tools\idf.py" -C firmware/lpg_display build
```

Result:

```text
lpg_display.bin binary size 0x139350 bytes.
Smallest app partition is 0x400000 bytes. 0x2c6cb0 bytes (69%) free.
```

The display build required manual ESP-IDF tool path setup in this shell because `IDF_PATH`/export was not usable directly:

```powershell
$env:IDF_PATH='C:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\python_env\idf5.5_py3.11_env'
$env:PATH='C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;' + $env:PATH
& 'C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe' 'C:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py' -C firmware/lpg_display build
```

## Production Versus Prototype Verification

Production behavior:

- `LPG_MODBUS_WRITES_ENABLED` defaults to `0`.
- Modbus writes are disabled unless explicitly enabled.
- Production build compiles successfully without dev flags.
- Production STA defaults are empty; old `Rao` / `password123` values are not production defaults.

Prototype behavior:

- `-DLPG_PROTOTYPE_BUILD=1` enables Modbus writes.
- Prototype build compiles successfully.
- Start Fill writes are available in prototype because the Modbus command register path is compiled in.
- Dev WiFi defaults are available only with `-DLPG_DEV_WIFI_DEFAULTS=1`.

## 2026-05-08 Flash Attempt After Resource Register Sync

Flashed:

- Controller prototype/write-enabled firmware to `COM10`.
- Controller SPIFFS web assets to `COM10`.
- Display firmware to `COM8`.

Flash evidence:

```text
Controller COM10: Wrote 1243600 bytes at 0x00010000. Hash of data verified.
SPIFFS COM10: Wrote 1441792 bytes at 0x00290000. Hash of data verified.
Display COM8: Wrote lpg_display.bin at 0x00010000. Hash of data verified.
```

Post-flash controller serial evidence:

```text
[SERIAL] rtu: enabled=1 slave=1 baud=9600 data=8 parity=0 stop=1 rx=14 tx=27 de=255
[STATUS] state=IDLE live=-0.030 tare=0.000 net=0.000 target=0.000 rate=356.00 amount=0.00 nozzle=1 cylinder=1 estop=1 reason=
[IO] interpreted cylinder=1 nozzle=1 estopOk=1
```

Post-flash display serial evidence showed the RTU link was not stable enough to claim hardware verification:

```text
I (...) MBUS: RTU health=UNSTABLE ok=1 fail=0 ...
I (...) MBUS: RTU health=OFFLINE ok=2 fail=8 ...
W (...) MBUS: RTU timeout/no-rx rx=0 expect=53 req=01 03 00 00 00 18 45 C0
```

Result: flash was performed, but Device ID, preset sync, Start Fill, and resource-register reads are not hardware verified for this change set. Do not claim hardware completion from this log.

## Verified And Not Yet Verified

Verified on hardware:

- Controller alive on `COM10`.
- Controller RTU settings are correct.
- Controller readiness inputs were OK.
- Display alive on `COM8`.
- Display RTU health reached `ONLINE`.
- Display read Device ID `0xA601` from register `0x0018`.

Verified by build/code inspection:

- Production build has Modbus writes disabled by default.
- Production FC06 and FC16 writes return Modbus exceptions when writes are disabled or rejected.
- Prototype build has Modbus writes enabled with `-DLPG_PROTOTYPE_BUILD=1`.
- Display Start Fill code writes target, rate, amount, reads back registers `0x0006`, `0x0008`, `0x000A`, then writes command and confirms state transition.
- FaultScreen uses alarm code text via shared UI helpers.
- Controller exposes board resource/status registers from `0x0051` through `0x0076`.
- Controller webpage route `/modbus-map` serves the active register table.

Remaining hardware tests:

- Use Modbus Poll or equivalent hardware link to confirm Device ID `0x0018 = 0xA601` after this change set.
- Confirm target `12.000` at `0x0006`, rate `250.000` at `0x0008`, target amount `3000.000` at `0x000A`, and current amount at `0x000C` after preset sync.
- Confirm resource registers `0x0051..0x0077` return sensible values.
- Force exactly one missed RTU frame and confirm the display reports `UNSTABLE`, not `OFFLINE`.
- Disconnect/disable controller or RS485 long enough to confirm display reports `OFFLINE`.
- Press Start Fill and capture logs showing target/rate/amount/command writes and confirm-state behavior.
- Trigger a real fault and confirm `FaultScreen` shows the real alarm reason.

## Notes For Future Agents

- Do not treat the lack of old `0x1001` registers as a problem. The active map is 0-based `0x0000..0x0077`.
- Do not treat `Rao` / `password123` as production credentials. They are dev-only behind a build flag.
- If display serial moves from `COM8`, check Windows serial enumeration before assuming the board is dead.
- If `idf.py flash` times out, direct `esptool` at `115200` has been reliable.
