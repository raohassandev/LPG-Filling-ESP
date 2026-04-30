# KC868-A6 LPG Controller Firmware

This sketch is the custom firmware baseline for the LPG filling project.

Current scope:

- verified KC868-A6 I2C addresses and relay behavior modeled in code
- safe boot path with all relays forced to the inactive state
- default STA mode with fallback AP for device-hosted UI
- OLED status display for IP address and controller state
- serial-first diagnostics
- status API and minimal HMI
- simulated weight path for early logic and UI testing
- serial command interface for start, stop, reset, and simulated weight
- persistent AP and slow-fill settings load path
- local append-only event log exposed by API
- transaction records stored on SPIFFS and exposed by API

Hardware integration reference:

- [Load Cell and HX711 Connection Guide](../../docs/load_cell_hx711_connection_guide.md)
- [Prototype Execution Plan](../../docs/prototype_execution_plan.md)
- [Development Plan](../../LPG_Filling_Station_Development_Plan.md)

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
- `cal <factor>`
- `sim 5.25`
- `start 11.8 250`
- `stop`
- `reset`

Useful HTTP endpoints:

- `GET /api/status`
- `GET /api/settings`
- `GET /api/logs`
- `GET /api/transactions`
- `GET /api/transactions.csv`
- `POST /api/start`
- `POST /api/stop`
- `POST /api/reset`
- `POST /api/sim-weight`

Default WiFi:

- STA SSID: `Rao`
- STA password: `password123`
- fallback AP SSID: `LPG-Controller-Setup`

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

Recommended next implementation steps:

1. verify input truth table on real hardware
2. verify relay truth table on real hardware
3. verify HX711 pin availability and calibration on the actual KC868-A6 wiring
4. harden process state machine and fault handling
5. finalize production security and network mode policy

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
