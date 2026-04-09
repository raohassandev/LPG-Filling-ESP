# Hardware Verification Checklist

## Purpose

Use this checklist before the first LPG firmware flash and again before any live actuator testing.

## Board Safety

- Confirm the connected KC868-A6 is the intended development unit.
- Confirm a full flash backup exists and its checksum is recorded.
- Confirm no hazardous actuators are connected during early boot and relay testing.
- Confirm a physical emergency isolation method exists outside firmware.

## USB and Serial

- Confirm serial port appears as `/dev/cu.usbserial-110` or current assigned port.
- Confirm ESP32 bootloader responds to `esptool`.
- Confirm serial boot logs are readable at `115200`.

## Relay Verification

- Verify each relay output against a safe indicator or multimeter.
- Verify active-low logic against physical behavior.
- Verify all relays remain inactive on boot with project firmware.
- Verify no boot self-test energizes outputs unintentionally.

## Input Verification

- Verify cylinder-present input truth state.
- Verify nozzle-engaged input truth state.
- Verify emergency-stop input truth state.
- Verify debounce behavior with repeated toggling.
- Verify loss-of-signal behavior for safety inputs.

## I2C Devices

- Verify device `0x22` responds.
- Verify device `0x24` responds.
- Verify device `0x3C` responds.
- Verify device `0x68` responds.

## Network

- Verify fallback AP starts when station Wi-Fi is unavailable.
- Verify AP IP address is reachable.
- Verify station Wi-Fi credentials can be updated safely.
- Verify local web UI remains non-authoritative for process safety.

## Storage and Flash

- Verify partition table matches intended firmware layout.
- Verify SPIFFS upload path works.
- Verify settings persistence survives reboot.
- Verify failed write or reboot does not corrupt startup state.

## Before Live Process Testing

- Confirm relay truth table is documented.
- Confirm input truth table is documented.
- Confirm nozzle disengagement causes safe stop.
- Confirm emergency-stop causes safe state immediately.
- Confirm operator and maintenance actions are restricted as designed.
