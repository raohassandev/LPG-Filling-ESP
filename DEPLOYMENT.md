# Deployment Guide — KC868-A6 LPG Controller

## Build Flags

| Flag | Production | Dev/Test |
|------|-----------|---------|
| `LPG_MODBUS_WRITES_ENABLED` | `0` (default) | `1` only for SCADA integration with auth |
| `LPG_DEV_BUILD` | **not defined** | Defined to enable sim endpoints |

In Arduino IDE: Sketch → `#define` at top of `.ino`, or add to `build_flags` in platformio.ini.

Never define `LPG_DEV_BUILD` in production firmware.

## Firmware Upload

1. Install Arduino IDE 2.x with ESP32 board support (Espressif ESP32 3.x)
2. Required libraries:
   - `WebSocketsServer` (Links2004)
   - `PubSubClient` (Nick O'Leary)
   - `Adafruit SSD1306` + `Adafruit GFX`
   - `RTClib` (Adafruit)
3. Open `firmware/lpg_controller/lpg_controller.ino`
4. Select board: `ESP32 Dev Module`, Flash: 4MB, Partition: project partition CSV / SPIFFS-capable layout
5. Upload

## Filesystem Upload (SPIFFS)

The web portal UI (`/index.html`) is served from SPIFFS.
1. Install the ESP32 Sketch Data Upload plugin
2. Place `index.html` in the `data/` folder
3. Tools → ESP32 Sketch Data Upload

If no `index.html` is present, the firmware still runs — the API is accessible without the web portal.

## First Boot Setup

1. Power on and connect to the device AP: `LPG-XXXXXX` (SSID printed on Serial)
2. AP password is `LP<XX><XX><XX>` (last 3 MAC bytes, printed to Serial on boot)
3. Navigate to `http://192.168.4.1` (default AP IP)
4. Log in as `admin` with the one-time password printed to Serial
5. **Change the admin password immediately**
6. Create operator and manufacturer accounts as needed
7. Configure WiFi STA credentials to connect to site network
8. Perform scale calibration via the Diagnostics tab (Manufacturer role)
9. Verify fill start gate checks pass before connecting any fill hardware

## Mobile App Configuration

1. Install the Expo app from source: `cd apps/lpg-expo-app && yarn start`
2. Default device URL: `http://lpg-controller.local` (mDNS) or use the STA IP
3. Log in with operator/admin/manufacturer credentials
4. The app will not start fill unless firmware reports `readyToFill: true`

## Checklist Before Controlled Dry Test

- [ ] Firmware flashed without `LPG_DEV_BUILD`
- [ ] `LPG_MODBUS_WRITES_ENABLED=0` (default)
- [ ] Admin password changed from first-boot value
- [ ] Site WiFi, MQTT, and user credentials changed from development defaults
- [ ] Config export created with `GET /api/config/export` for handover backup
- [ ] Commissioning summary checked with `GET /api/commissioning`
- [ ] Scale calibrated and `calValid=true` in `/api/status`
- [ ] E-stop test: open E-stop input → verify all relays de-energize and state=FAULT
- [ ] Fill start block test: each gate condition blocks start individually
- [ ] Fill completion test: settling state reached, transaction logged
- [ ] Overfill test: manually advance net weight past target+500g in sim mode → fault
- [ ] Timeout test: fill without weight change → no-flow fault within 10s window

## Checklist Before LPG Pilot

All dry-test checklist items **plus**:
- [ ] Hardwired E-stop installed and independently tested
- [ ] Fail-closed valves verified
- [ ] Load cell certified with traceable reference weights
- [ ] Input truth table verified with actual hardware
- [ ] Qualified LPG engineer sign-off
- [ ] Regulatory compliance review complete
