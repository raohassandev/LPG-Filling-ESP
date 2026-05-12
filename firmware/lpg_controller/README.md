# KC868-A6 LPG Controller Firmware

This sketch is the custom firmware baseline for the LPG filling project.

Current scope:

- verified KC868-A6 I2C addresses and relay behavior modeled in code
- safe boot path with all relays forced to the inactive state
- default STA mode with fallback AP for device-hosted UI
- OLED status display for IP address and controller state
- serial-first diagnostics
- status API and minimal firmware-hosted API landing page
- Android Expo app for operator, admin, and manufacturer UI
- simulated weight path for early logic and UI testing
- serial command interface for start, stop, reset, and simulated weight
- persistent AP and slow-fill settings load path
- local append-only event log exposed by API
- transaction records stored on SPIFFS and exposed by API
- operator tare weight, net weight, and net-weight price calculation
- admin rate setup and local sales statistics
- active Modbus RTU register map for display/HMI integration
- optional Modbus TCP server on port `502`

Hardware integration reference:

- [Load Cell and HX711 Connection Guide](../../docs/load_cell_hx711_connection_guide.md)
- [Prototype Execution Plan](../../docs/prototype_execution_plan.md)
- [Development Plan](../../LPG_Filling_Station_Development_Plan.md)
- [Modbus and Real-Time Protocol](../../docs/modbus_and_realtime_protocol.md)

Not complete yet:

- HX711 calibration on the actual KC868-A6 wiring
- RTC hardware validation
- finalized input truth table
- production security
- finalized network mode policy

Suggested compile target:

- FQBN: `esp32:esp32:esp32`
- preferred CLI in this repo: `tools/local/arduino-cli.exe`

Serial commands after flashing:

- `help`
- `status`
- `hx`
- `weight`
- `tare`
- `tarew <emptyCylinderKg>`
- `zeronet`
- `cal <factor>`
- `sim 5.25`
- `start 11.8 250`
- `stop`
- `reset`

Useful HTTP endpoints:

- `GET /api/status`
- `GET /api/settings`
- `GET /api/modbus`
- `GET /api/logs`
- `GET /api/transactions`
- `GET /api/transactions.csv`
- `POST /api/start`
- `POST /api/tare`
- `POST /api/tare-zero`
- `POST /api/settings`
- `POST /api/stop`
- `POST /api/reset`
- `POST /api/sim-weight`

Default WiFi:

- STA credentials are commissioning values and should be changed per site.
- The fallback AP uses a device-unique password printed on first boot.
- fallback AP SSID: `LPG-Controller-Setup`

Android app:

- path: `apps/lpg-expo-app`
- run: `npm install`, then `npm run android`
- default device URL: `http://192.168.0.108`
- real-time behavior: tries `ws://<device-ip>/ws`, then falls back to `GET /api/status` every 1 second

Relay map:

- Relay 1 / Output 1: fast fill valve
- Relay 2 / Output 2: slow fill valve
- Relay 3 / Output 3: main supply valve
- Relay 4 / Output 4: pump / compressor
- Relay 5 / Output 5: alarm horn / beacon
- Relay 6 / Output 6: status indicator

Fill sequence:

- Fast fill energizes Relay 1 and Relay 3.
- Slow fill turns Relay 1 off and energizes Relay 2 and Relay 3.
- Stop, complete, abort, and fault force all relays off.
- Fill thresholds and transaction totals use net weight, not gross/live cylinder weight.
- Operator amount mode calculates target weight as `amount / ratePerKg`.
- Operator weight mode calculates target amount as `weight * ratePerKg`.

Active Modbus/HMI register map:

- Active display path: Modbus RTU over RS485 using 0-based holding register addresses in `include/ModbusRegisterMap.h`.
- Recommended RTU settings: slave `1`, `115200`, `8N1`. Factory default (no NVS key) uses 115200. Devices with a previously saved NVS baud (e.g. 9600) retain that value across firmware updates — use the "Apply Recommended 115200" action on the Modbus page to reset to 115200 and restart.
- Display reads live/tare/net/target/rate/amount/state/readiness from `0x0000..0x0017`.
- Display reads diagnostics from `0x0048..0x0050`.
- Optional Modbus TCP remains available on port `502` for integration clients.
- The older `0x1001..0x1006` prototype map is legacy reference only. Do not use it for current display firmware.

Recommended next implementation steps:

1. verify input truth table on real hardware
2. verify relay truth table on real hardware
3. verify HX711 pin availability and calibration on the actual KC868-A6 wiring
4. verify active RTU register map against display and third-party HMI tools
5. harden process state machine and fault handling
6. finalize production security and network mode policy

Windows build helper from the repo root:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_firmware.ps1
```

Windows upload helpers from the repo root:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\upload_firmware.ps1 -Port COM6
powershell -ExecutionPolicy Bypass -File .\scripts\upload_spiffs.ps1 -Port COM6
```

## Terminal Commands

Run all commands from the repository root:

```powershell
cd F:\Working\LPG-Filling-ESP
```

List connected serial ports:

```powershell
.\tools\local\arduino-cli.exe board list
```

If the board appears on another port, replace `COM6` in the commands below.

Compile firmware:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_firmware.ps1
```

Run prototype source-contract checks:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test_prototype_contracts.ps1
```

Download/upload firmware to the KC868-A6:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\upload_firmware.ps1 -Port COM6
```

Download/upload the web UI to SPIFFS:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\upload_spiffs.ps1 -Port COM6
```

Open a simple interactive serial console:

```powershell
powershell -ExecutionPolicy Bypass -Command '$p = New-Object System.IO.Ports.SerialPort "COM6",115200,"None",8,"One"; $p.Open(); Write-Host "Serial open. Type help, hx, status, tare, reset, or exit."; while ($true) { $cmd = Read-Host ">"; if ($cmd -eq "exit") { break }; $p.WriteLine($cmd); Start-Sleep -Milliseconds 1200; $text = $p.ReadExisting(); if ($text.Length -gt 0) { Write-Host $text } else { Write-Host "[no response]" } }; $p.Close()'
```

Quick one-shot HX711/status test:

```powershell
powershell -ExecutionPolicy Bypass -Command '$p = New-Object System.IO.Ports.SerialPort "COM6",115200,"None",8,"One"; $p.Open(); Start-Sleep -Milliseconds 500; foreach ($cmd in @("hx","status")) { Write-Host "> $cmd"; $p.WriteLine($cmd); Start-Sleep -Milliseconds 1200; $text = $p.ReadExisting(); if ($text.Length -gt 0) { Write-Host $text } }; $p.Close()'
```

Quick tare and status test:

```powershell
powershell -ExecutionPolicy Bypass -Command '$p = New-Object System.IO.Ports.SerialPort "COM6",115200,"None",8,"One"; $p.Open(); Start-Sleep -Milliseconds 500; foreach ($cmd in @("reset","tare","hx","status")) { Write-Host "> $cmd"; $p.WriteLine($cmd); Start-Sleep -Milliseconds 1500; $text = $p.ReadExisting(); if ($text.Length -gt 0) { Write-Host $text } }; $p.Close()'
```

Check API status from the browser/network:

```powershell
Invoke-RestMethod -Uri http://192.168.0.108/api/status | ConvertTo-Json -Depth 4
```

Check transaction history:

```powershell
Invoke-RestMethod -Uri http://192.168.0.108/api/transactions | ConvertTo-Json -Depth 5
```



Flash Firmware to KC868-A6
Step 1 — Install Arduino IDE 2.x
Download from arduino.cc if not already installed.

Step 2 — Add ESP32 Board Support
In Arduino IDE: File → Preferences → Additional Boards Manager URLs, add:


https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
Then Tools → Board → Boards Manager, search esp32, install "esp32 by Espressif Systems" (3.x).

Step 3 — Install Required Libraries
Tools → Manage Libraries, install exactly these two:

Library	Author	Search term
WebSockets	Markus Sattler (Links2004)	WebSocketsServer
PubSubClient	Nick O'Leary	PubSubClient
No other external libraries needed — HX711, OLED, and RTC are all implemented directly in the firmware.

Step 4 — Open the Sketch
File → Open → navigate to:


firmware/lpg_controller/lpg_controller.ino
Step 5 — Configure Board Settings
Tools menu — set these exactly:

Setting	Value
Board	ESP32 Dev Module
Upload Speed	921600
CPU Frequency	240MHz (WiFi/BT)
Flash Mode	QIO
Flash Size	4MB (32Mb)
Partition Scheme	Custom
Core Debug Level	None
PSRAM	Disabled
The partitions.csv in the sketch folder is picked up automatically when Custom is selected.

Step 6 — Connect the Board
USB-to-serial cable to KC868-A6 UART0 (or built-in USB if your revision has it)
Tools → Port → select the COM port that appears
Step 7 — Upload
Click Upload (→ arrow button). Arduino will compile and flash. Takes ~30-60 seconds.

Step 8 — First Boot
Open Tools → Serial Monitor at 115200 baud. You'll see:


[BOOT] First boot — admin password: xxxxxxxx
[BOOT] AP SSID: LPG-XXXXXX  password: LPXXXXXX
Write down the admin password — it's printed only once (unless you erase NVS).

Step 9 — SPIFFS Upload (Web Portal UI) — Optional
If you want the web portal served from the device:

Install ESP32 Sketch Data Upload plugin
Place index.html in firmware/lpg_controller/data/
Tools → ESP32 Sketch Data Upload
If no index.html is present, the REST API still works — only the browser portal is missing.
