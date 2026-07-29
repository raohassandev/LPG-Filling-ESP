# KC868-A6 LPG Controller OTA Update Guide

## Scope

This OTA implementation updates the ESP32 application image through the controller's local web server. It does not update SPIFFS data, the bootloader, or the partition table.

The implementation is isolated on branch:

`agent/ota-update-service`

## Safety Rules

- OTA is accepted only while the process state is `IDLE`.
- Entering OTA changes the process state to `MAINTENANCE`.
- Fill start, reset, stop, and simulated-weight HTTP actions are locked while an update is active or a reboot is pending.
- The fill controller also rejects start/reset commands while the controller is in `MAINTENANCE`, including commands arriving from serial or another interface.
- The existing dual-slot partition table is retained: `app0` and `app1` are each `0x140000` bytes.
- Keep controller power stable for the complete upload and reboot cycle.
- For early verification, keep hazardous actuators disconnected or independently isolated.

## Authentication

The upload requires the `X-Manufacturing-PIN` HTTP header.

The PIN is loaded from ESP32 Preferences namespace `lpgctrl`, key `mfg_pin`.

The temporary default is:

`2468`

The PIN must contain 4 to 8 digits. A site/manufacturing provisioning step must replace the temporary default before production deployment.

## Build the OTA Binary

From the repository root:

```bash
chmod +x scripts/build_ota_firmware.sh
./scripts/build_ota_firmware.sh
```

The script:

- uses the repo-local Arduino CLI when available
- compiles the KC868-A6 controller sketch
- exports the application binary
- checks that it fits the `0x140000` OTA application slot
- prints a SHA-256 checksum when a checksum utility is available

Expected application binary:

```text
firmware/kc868_a6_lpg_controller/build/ota/kc868_a6_lpg_controller.ino.bin
```

Upload only this `.ino.bin` application file. Do not upload bootloader, partition, SPIFFS, merged-flash, or other generated binaries through the OTA page.

## Browser Update Procedure

1. Connect to the controller access point.
2. Open the controller IP address shown in the serial log, normally the SoftAP gateway address.
3. Open `/ota`, for example `http://192.168.4.1/ota`.
4. Confirm the page reports controller state `IDLE`.
5. Enter the manufacturing PIN.
6. Select `kc868_a6_lpg_controller.ino.bin`.
7. Start the upload and keep power connected.
8. Wait for the JSON success response and automatic reboot.
9. Reconnect and verify `/api/version` reports the expected firmware version.
10. Review `/api/logs` for `ota_start` and `ota_complete`.

## API Endpoints

### OTA Status

```http
GET /api/ota/status
```

Reports device name, firmware version, process state, update permission, busy state, and available sketch space.

### OTA Upload

```http
POST /api/ota/upload
X-Manufacturing-PIN: 2468
Content-Type: multipart/form-data
```

Form field name:

`firmware`

The firmware must be an ESP32 application image whose first byte is the ESP image magic value and whose total size fits the inactive OTA slot.

## Verification Checklist

### Negative Tests

- incorrect PIN returns HTTP `401`
- missing/non-`.bin` filename is rejected
- upload while state is not `IDLE` is rejected
- non-ESP32 binary is rejected before flash write continues
- oversized/corrupt image is rejected by the ESP32 Update library
- start/reset commands are rejected during `MAINTENANCE`
- interrupted upload leaves the current running application intact

### Positive Test

- build a firmware with a visibly different version string
- upload it from `/ota`
- confirm success response
- confirm automatic reboot
- confirm new version from `/api/version`
- confirm relays remain inactive throughout boot and update testing

## Current Limitation

This first OTA implementation relies on the ESP32 dual application slots and Update library verification. It does not yet add signed-image verification, remote cloud delivery, staged fleet rollout, or automatic application rollback confirmation. Those should be separate hardening stages after local hardware OTA passes.
