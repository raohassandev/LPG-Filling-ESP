# LPG Filling Commissioning Guide

Use this checklist after flashing the controller and display firmware.

## 0. Device Credentials

| Item | Value |
| --- | --- |
| Controller IP | 192.168.0.100 |
| Modbus TCP Port | 502 |
| Manufacturing PIN | **242728** |
| PIN location | NVS namespace `lpgmfg`, key `pin` |

The Manufacturing PIN is required to change RTU settings, run calibration, and upload OTA firmware via the web portal (`X-MFG-PIN` HTTP header).  
To recover a lost PIN: dump NVS partition at flash `0x9000` size `0x5000` with esptool and search for namespace `lpgmfg`.

## 1. Serial Basics

Controller serial: `115200 baud`.

Useful commands:

```text
help
io
status
hx
rtu
reset
start 12 250
```

## 2. Input Verification

Run `io` on controller serial and toggle each real input.

Expected current prototype mapping:

| Function | Controller input |
| --- | --- |
| Cylinder present | IN1 |
| Nozzle engaged | IN2 |
| E-stop raw/tripped | IN4 |

Acceptance:

- Removing cylinder changes interpreted `cylinder=0`.
- Unlocking nozzle changes interpreted `nozzle=0`.
- Pressing E-stop changes interpreted `estopOk=0`.
- Releasing E-stop plus reset returns the system to ready when all other blockers are clear.

## 3. RS485 Verification

Known-good link:

| Item | Value |
| --- | --- |
| Controller slave | `1` |
| Baud | `9600` |
| Format | `8N1` |
| Display TX/RX | GPIO44/GPIO43 |
| Controller TX/RX | GPIO27/GPIO14 |

On the display, open Admin Settings -> Controller Link and press Test Link.

## 4. Readiness And Fill

Before live LPG testing:

- E-stop OK.
- Cylinder present.
- Nozzle engaged.
- Scale initialized.
- Scale stable.
- Calibration valid.
- No active alarm.

Use dry testing first. Confirm relay behavior with valves disconnected or safely isolated.

## 5. Handover Data

After setup, export configuration:

```text
GET /api/config/export
```

Check commissioning status:

```text
GET /api/commissioning
```

The export intentionally excludes WiFi and MQTT passwords.

## 6. Relay Pulse Test

For maintenance-only output verification:

```text
POST /api/relay/test-pulse?relay=1&durationMs=500
```

Rules:

- `relay` is 1-based: `1..6`.
- `durationMs` defaults to `500` and is capped at `2000`.
- The request is rejected during active fill, validation, or settling.
- All relays are forced off after the pulse.
