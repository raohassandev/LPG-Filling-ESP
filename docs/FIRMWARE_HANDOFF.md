# Firmware Handoff

Current date: 2026-05-08

This document is the firmware handoff for future coding agents. It documents the current working state without requiring chat history.

## Scope

Active firmware projects:

- Controller: `firmware/lpg_controller`
- Display: `firmware/lpg_display`

Do not use the old `firmware/kc868_a6_lpg_controller` path. It is stale.

## Repository Structure

Important paths:

- `AI-Context.md` - short entry-point context for agents
- `docs/BOARD_REFERENCE.md` - board pin and flashing reference
- `docs/FIRMWARE_HANDOFF.md` - this document
- `docs/FIRMWARE_BENCH_VERIFICATION_LOG.md` - bench evidence and remaining tests
- `docs/MODBUS_RTU_CURRENT_MAP.md` - active RTU map summary
- `docs/modbus_and_realtime_protocol.md` - protocol detail
- `firmware/lpg_controller` - Arduino ESP32 controller firmware
- `firmware/lpg_display` - ESP-IDF ESP32-S3 display firmware
- `scripts/build_firmware.ps1` - controller production build helper
- `tools/local/arduino-cli.exe` - repo-local Arduino CLI

Non-firmware app/docs may exist, but do not edit them when the task is firmware-only.

## Controller Firmware

Path: `firmware/lpg_controller`

Build system: Arduino CLI.

Primary sketch:

- `firmware/lpg_controller/lpg_controller.ino`

Important modules:

- `include/BoardConfig.h` - KC868-A6 pins and build-flag guarded defaults
- `include/ModbusRegisterMap.h` - active 0-based Modbus register addresses
- `src/ModbusRegisterMap.cpp` - Modbus read/write behavior and production/prototype write gating
- `src/ModbusRtuService.cpp` - Modbus RTU server
- `src/ResourceMonitor.cpp` - controller runtime/resource monitor exposed through Modbus
- `src/FillController.cpp` - filling state machine
- `src/SettingsStore.cpp` - NVS/Preferences settings
- `src/NetworkManager.cpp` - WiFi AP/STA management
- `src/WebPortal.cpp` - HTTP/API/web UI backend
- `src/WeightService.cpp` - HX711 integration
- `src/InputExpander.cpp` - PCF8574 input reads
- `src/RelayBank.cpp` - PCF8574 relay writes
- `src/RtcService.cpp` - DS3231 RTC

### Controller Hardware

Board: KC868-A6, ESP32-D0WD-V3.

Known-good pin/resource mapping:

| Resource | Value |
|---|---|
| I2C SDA | GPIO4 |
| I2C SCL | GPIO15 |
| Relay expander | PCF8574 at `0x24` |
| Input expander | PCF8574 at `0x22` |
| OLED | SSD1306 at `0x3C` |
| RTC | DS3231 at `0x68` |
| RS485 RX | GPIO14 |
| RS485 TX | GPIO27 |
| RS485 DE/RE | none, auto-direction transceiver |

Current verified input mapping:

| Signal | Input |
|---|---|
| Cylinder present | IN1 / index 0 |
| Nozzle engaged | IN2 / index 1 |
| E-stop | IN4 / index 3, raw active means tripped |

### Controller Build And Flash

Production build:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_firmware.ps1
```

Production flash:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\upload_firmware.ps1 -Port COM10
```

Prototype build with Modbus writes and dev WiFi defaults:

```powershell
.\tools\local\arduino-cli.exe compile --fqbn esp32:esp32:esp32 --build-property "compiler.cpp.extra_flags=-DLPG_PROTOTYPE_BUILD=1 -DLPG_DEV_WIFI_DEFAULTS=1" firmware\lpg_controller
```

Prototype flash:

```powershell
.\tools\local\arduino-cli.exe compile --fqbn esp32:esp32:esp32 --build-property "compiler.cpp.extra_flags=-DLPG_PROTOTYPE_BUILD=1 -DLPG_DEV_WIFI_DEFAULTS=1" --upload -p COM10 firmware\lpg_controller
```

### Production Versus Prototype Flags

Production default:

- `LPG_MODBUS_WRITES_ENABLED` defaults to `0`.
- Modbus holding-register writes return false.
- Production must not have hardcoded STA WiFi credentials.

Prototype/dev:

- `-DLPG_PROTOTYPE_BUILD=1` enables Modbus writes.
- `-DLPG_MODBUS_WRITES_ENABLED=1` also enables writes explicitly.
- `-DLPG_DEV_WIFI_DEFAULTS=1` enables old dev STA defaults.

Do not change the production default to writable.

## Display Firmware

Path: `firmware/lpg_display`

Build system: ESP-IDF v5.5.4.

Important files:

- `main/DisplayConfig.h` - display board and RS485 pin config
- `main/ModbusClient.h/.cpp` - display-side RTU client
- `main/DisplaySettings.h/.cpp` - display NVS settings store
- `main/screens/DashboardScreen.cpp` - main operator screen
- `main/screens/SettingsScreen.cpp` - Settings and Controller Link / RS485 tab
- `main/screens/FaultScreen.cpp` - fault/alarm screen
- `sdkconfig.defaults` and `sdkconfig` - active ESP-IDF config

### Display Hardware

Board: Waveshare ESP32-S3 Touch LCD 5-inch class board.

Known-good details:

| Resource | Value |
|---|---|
| MCU | ESP32-S3 |
| LCD | RGB 800x480 |
| Touch | GT911 over I2C |
| PSRAM | OPI PSRAM, 8 MB |
| Flash | 16 MB |
| USB console | USB Serial/JTAG |
| RS485 UART | UART1 |
| RS485 TX | GPIO44 |
| RS485 RX | GPIO43 |
| RTU default | slave `1`, `9600 8N1` |

Do not swap GPIO43/GPIO44 based only on schematic net naming. The active working display configuration uses TX=44 and RX=43.

### Display Build And Flash

Build:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
$env:IDF_PATH='C:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\python_env\idf5.5_py3.11_env'
. "$env:IDF_PATH\export.ps1"
python "$env:IDF_PATH\tools\idf.py" -C firmware/lpg_display build
```

Flash current bench display:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
$env:IDF_PATH='C:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\python_env\idf5.5_py3.11_env'
. "$env:IDF_PATH\export.ps1"
python -m esptool --chip esp32s3 -p COM8 -b 115200 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 firmware\lpg_display\build\bootloader\bootloader.bin 0x10000 firmware\lpg_display\build\lpg_display.bin 0x8000 firmware\lpg_display\build\partition_table\partition-table.bin 0xe000 firmware\lpg_display\build\ota_data_initial.bin
```

Serial monitor:

```powershell
python - <<'PY'
import serial, time
ser = serial.Serial('COM8', 115200, timeout=0.2)
end = time.time() + 30
while time.time() < end:
    data = ser.read(4096)
    if data:
        print(data.decode('utf-8', 'replace'), end='')
ser.close()
PY
```

## Active Modbus RTU Map

The active Modbus map is 0-based and defined in `firmware/lpg_controller/include/ModbusRegisterMap.h`.

Use these PDU addresses with Function 03/04/06/16. Some Modbus tools display Holding Register addresses as 40001 + PDU address.

Old 0x1001 map is legacy only and must not be used for ESP32-S3 display, Haiwell HMI, Modbus Poll verification, or current SCADA integration.

Critical registers:

| Decimal Address | Hex Address | Name | Type | Access | Registers |
|---:|---|---|---|---|---:|
| 0000 | `0x0000` | Live Weight | Float32 | R | 2 |
| 0006 | `0x0006` | Target Weight | Float32 | RW in prototype | 2 |
| 0008 | `0x0008` | Rate Per Kg | Float32 | RW in prototype | 2 |
| 0010 | `0x000A` | Target Amount | Float32 | RW in prototype | 2 |
| 0012 | `0x000C` | Current Amount | Float32 | R | 2 |
| 0014 | `0x000E` | Fill State | UINT16 | R | 1 |
| 0023 | `0x0017` | Command | UINT16 | W in prototype | 1 |
| 0024 | `0x0018` | Device ID | UINT16 | R, `0xA601` | 1 |
| 0072 | `0x0048` | Alarm Code | UINT16 | R | 1 |
| 0074 | `0x004A` | Readiness Mask | UINT16 | R | 1 |
| 0075 | `0x004B` | Blocker Mask | UINT16 | R | 1 |
| 0081 | `0x0051` | Application Load Percent | Float32 | R | 2 |
| 0087 | `0x0057` | Heap Total Memory | UINT32 | R | 2 |
| 0089 | `0x0059` | Heap Free Memory | UINT32 | R | 2 |
| 0107 | `0x006B` | ESP32 Internal Chip Temperature | Float32 | R | 2 |
| 0109 | `0x006D` | WiFi RSSI | INT16 | R | 1 |
| 0110 | `0x006E` | WiFi Status | UINT16 | R | 1 |
| 0114 | `0x0072` | Modbus RTU Request Count | UINT32 | R | 2 |
| 0116 | `0x0074` | Modbus RTU Error Count | UINT32 | R | 2 |
| 0118 | `0x0076` | Controller Heartbeat Counter | UINT32 | R | 2 |

The full table is in `docs/user/MODBUS_PROTOCOL.md` and the controller webpage route `/modbus-map`.

## Display Controller Link / RS485 Settings

The display Settings screen uses `DisplaySettingsStore` for:

- `slaveAddress`
- `baudRate`
- `parity`
- `stopBits`
- `timeoutMs`
- `retries`
- `unstableDebounceMs`
- `offlineDebounceMs`

Save applies settings with `ModbusClient::applySettings()`.

Test reads Device ID register `0x0018` and expects `0xA601`.

## Rules Future Agents Must Not Break

- Do not edit firmware source when the user asks for documentation only.
- Do not reintroduce `firmware/kc868_a6_lpg_controller`.
- Do not use the old `0x1001` map for display or new HMI work.
- Do not enable Modbus writes in production by default.
- Do not restore `Rao` / `password123` as production WiFi defaults.
- Do not change display RS485 pins without proving the board wiring.
- Do not flash display firmware built for the wrong target or stale LCD pin map.
- Do not remove the Device ID check; it is the quickest bench proof that display and controller are speaking the same protocol.
- Do not leave relays energized during stop, abort, fault, reset, or failed validation paths.

## Remaining Hardware Tests

Still to perform deliberately on bench:

- Force one missed RTU frame and verify display logs/UI show `UNSTABLE`, not `OFFLINE`.
- Hold RS485/controller offline long enough to verify `OFFLINE`.
- Press Start Fill and capture COM8/COM10 logs confirming target/rate/amount/command writes and confirm-state behavior.
- Trigger a real fault and verify `FaultScreen` displays the real alarm reason text.
