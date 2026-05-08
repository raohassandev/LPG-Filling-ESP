# LPG Display Firmware

Active firmware for the Waveshare ESP32-S3-Touch-LCD-5 display.

## Active Communication

- Controller link: Modbus RTU over RS485.
- Display UART: `UART_NUM_1`.
- Display pins: TX GPIO44, RX GPIO43.
- Known-good defaults: slave `1`, `9600`, `8N1`, timeout `300 ms`, retries `2`.
- Settings are persisted in display NVS and can be changed from Admin Settings -> Controller Link.

## Build

Use the ESP-IDF environment configured for this repo:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
$env:IDF_PATH='C:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\python_env\idf5.5_py3.11_env'
. "$env:IDF_PATH\export.ps1"
python "$env:IDF_PATH\tools\idf.py" -C firmware/lpg_display build
```

## Flash

```powershell
python "$env:IDF_PATH\tools\idf.py" -C firmware/lpg_display -p COM9 flash
```

Use the actual COM port shown by Windows Device Manager or `idf.py -p <port> flash`.

## Field Checks

- Main header should show RS485 OK/UNSTABLE/OFFLINE.
- Live weight should update when controller polling is healthy.
- Start Fill should not show a hard offline error for a single missed RTU frame.
- Admin Settings -> Controller Link should show current link status and saved RTU settings.
