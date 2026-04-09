# KC868-A6 Hardware Profile

## 1. Purpose

This document is the verified hardware reference for the LPG filling project based on the KinCony KC868-A6 controller.

It exists to separate:

- confirmed board facts
- live inspection findings from the connected board
- implementation-relevant hardware constraints
- open items that still require verification

This file should be updated whenever new board-level evidence is collected from:

- official KinCony documentation
- schematic review
- serial boot logs
- physical inspection
- firmware probing
- electrical measurement

---

## 2. Document Status

- Status: working hardware profile
- Target board family: KinCony KC868-A6
- Host machine used for inspection: MacBook
- Inspection date: 2026-04-07
- Current confidence level: partial but strong for board identity and core peripherals

---

## 3. Verified Sources Used

The following sources were used to verify the information in this profile:

- KinCony KCS user guide
- KinCony KC868 smart controller parameter sheet
- KinCony KC868-A6 schematic
- direct USB/serial inspection on the connected board
- ESP32 bootloader probe via `esptool`
- live UART boot log capture

This profile should prefer measured or observed facts over brochure assumptions.

---

## 4. Board Identity

### 4.1 Board Model

- Board family: KinCony KC868-A6
- Controller class: ESP32-based industrial relay controller
- Intended mounting style: DIN-rail industrial controller family

### 4.2 MCU Identity

Verified from live `esptool` probe:

- MCU: `ESP32-D0WD-V3`
- Revision: `v3.1`
- CPU class: dual-core ESP32
- Features reported: WiFi, Bluetooth
- Crystal: `40 MHz`
- Flash size detected: `4 MB`
- MAC address: `38:18:2b:f0:ff:7c`

### 4.3 USB Interface

Verified from macOS USB inspection:

- Active serial device path: `/dev/cu.usbserial-110`
- USB bridge class: WCH CH340-class USB serial adapter
- USB product string shown by host: `USB Serial`

Implication:

- USB flashing and serial console access are available through the current host setup

---

## 5. Host Environment Verification

### 5.1 Development Host

- Operating system: macOS on MacBook
- Board connected successfully by USB
- Serial device visible to host

### 5.2 Installed Tools

- Arduino IDE installed
- `arduino-cli` available from Arduino IDE bundle
- Python `esptool` available locally

### 5.3 ESP32 Arduino Core Status

Observed on host:

- installed core: `esp32:esp32 2.0.0`
- latest index version visible to `arduino-cli`: `3.3.7`

Implication:

- the host can work with ESP32 now
- the installed ESP32 core is older and should not be assumed as the final project toolchain without review

### 5.4 Tooling Issue Observed

`arduino-cli` intermittently failed while installing or verifying `builtin:mdns-discovery` due to archive checksum mismatch.

Implication:

- USB serial workflows are still usable
- automatic network discovery via Arduino tooling is currently unreliable on this host

---

## 6. Official Board Resource Summary

The following are treated as verified board-level capabilities from official KinCony documentation and schematic review.

### 6.1 Digital I/O

- `6` relay outputs
- `6` digital inputs

### 6.2 Communication Interfaces

- WiFi
- Ethernet
- RS485
- RS232
- USB serial

### 6.3 Local Peripheral Features

- onboard OLED
- onboard RTC (`DS1307`)
- I2C expansion
- analog I/O support

### 6.4 Analog and Expansion Features

Documented in KinCony materials:

- `2` analog outputs `0-10V`
- analog inputs are present, but exact usable channel interpretation must be re-verified from board-level testing before freezing project design
- `2` free GPIOs indicated in the KinCony parameter sheet
- onboard SPI-related expansion support is present in schematic resources

### 6.5 Onboard Expansion and Peripheral Devices Seen in Schematic

- `PCF8574` I/O expanders
- `SSD1306` OLED
- `DS1307` RTC
- RS485 transceiver
- USB serial interface

---

## 7. Verified Runtime Findings From Connected Board

The following were observed directly from the connected controller over serial.

### 7.1 Current Firmware Identity

The connected board is **not** running stock KinCony KCS firmware at present.

Observed firmware banner:

```text
MAC-SYS Minimal v1.0.0
Initializing Industrial HVAC Controller
```

Implication:

- this board is already programmed with unrelated custom firmware
- current behavior cannot be assumed to match factory KinCony defaults
- the board must be treated as an in-use device until flash contents are backed up or intentionally replaced

### 7.2 I2C Runtime Configuration

Observed from boot log:

- I2C SDA pin: `GPIO4`
- I2C SCL pin: `GPIO15`
- I2C clock: `100000`

### 7.3 I2C Devices Detected At Boot

Observed from boot log:

- `0x22`
- `0x24`
- `0x3C`
- `0x68`

Working interpretation:

- `0x22`: onboard `PCF8574`
- `0x24`: second onboard `PCF8574`
- `0x3C`: OLED display
- `0x68`: `DS1307` RTC

This aligns strongly with official schematic evidence.

### 7.4 Relay Behavior Observed

Observed from boot log:

- relays handled as active-low
- firmware self-test toggled relays `0`, `1`, and `2`

Observed log phrases included:

- `All relays set to: 0x00 (logical), 0xFF (hardware)`
- `Relay 0 ON (MAC-SYS ACTIVE LOW)`
- `Relay 1 ON (MAC-SYS ACTIVE LOW)`
- `Relay 2 ON (MAC-SYS ACTIVE LOW)`

Critical implication:

- the current firmware performs relay self-test on boot
- this behavior is unsafe for LPG service if hazardous actuators are connected
- production LPG firmware must not energize process outputs during uncontrolled boot self-tests

### 7.5 RTC Behavior Observed

Observed from boot log:

- RTC detected successfully
- firmware reported a valid-looking timestamp

Observed time:

- `2025-10-07 16:42:54`

Implication:

- RTC hardware is responding
- current RTC time accuracy is untrusted until manually verified against real time

### 7.6 Network Behavior Observed

Observed from boot log:

- custom firmware attempts WiFi auto-connect
- configured hostname: `macsys-controller`
- saved AP name observed: `A12`

Implication:

- the board stores network configuration from prior firmware use
- existing network credentials/settings should be considered live device state, not project defaults

### 7.7 Configuration State Observed

Observed from boot log:

- configuration checksum mismatch detected
- firmware loaded defaults
- firmware saved configuration to EEPROM or equivalent storage

Implication:

- the currently loaded application uses persistent configuration storage
- storage layout and data format are specific to the existing custom firmware, not to the LPG project

---

## 8. Hardware Constraints Relevant To LPG Project

### 8.1 Not A Clean Board

This board is currently a functioning custom-controller target, not a blank hardware sample.

Required discipline before firmware replacement:

- back up flash contents if preservation matters
- record existing serial boot log
- record network behavior and any reachable interfaces
- decide whether this board is a development board or an operational board being repurposed

### 8.2 Limited Free GPIO Budget

Official documentation indicates very limited free GPIO on the A6 platform.

Implication:

- direct attachment of new peripheral hardware must not be assumed
- any design requiring additional signals must first verify actual free pins and signal conflicts
- the weight subsystem may need to remain external rather than directly attached

### 8.3 Active-Low Output Model

The board and current firmware behavior strongly indicate active-low output handling.

Implication:

- all output mapping documents must explicitly state logical state versus physical relay drive state
- safe-state logic must be verified on real hardware, not inferred from software booleans

### 8.4 Boot-Time Output Safety

Observed behavior proves that software can toggle outputs during initialization.

Implication:

- boot and reset behavior must be treated as a first-class safety requirement
- no LPG production firmware should be accepted until boot behavior is validated with actuators disconnected or safely simulated

### 8.5 Ethernet Should Not Be Ignored

Official documentation shows the A6 supports Ethernet and that KinCony’s own KCS workflow prefers Ethernet when both Ethernet and WiFi are configured.

Implication:

- production network policy should explicitly decide whether Ethernet is primary
- WiFi should not be treated as the only production network path

---

## 9. Preliminary Resource Mapping For Project Planning

This section is not final pin mapping. It is only a planning-level summary of what is believed available based on current evidence.

### 9.1 Confirmed Onboard Buses and Devices

- I2C bus in use by OLED, RTC, and I/O expanders
- relay outputs expanded through I2C hardware
- digital inputs likely routed through the same expansion scheme and opto-isolation chain
- RS485 hardware present
- USB serial available for flashing/debugging

### 9.2 Project-Relevant Implications

- use onboard OLED only for diagnostics/commissioning
- use relay outputs only after physical safe-state validation
- do not allocate I2C pins freely because onboard devices already occupy that bus
- do not assume analog channels are suitable for load-cell measurement
- prefer isolated or external weight subsystem until pin and performance budget are frozen

---

## 10. Unknowns That Still Need Verification

The following remain open and must be confirmed before implementation decisions are frozen.

### 10.1 Board Resource Unknowns

- exact analog input count actually usable on KC868-A6 in custom firmware context
- exact analog input ranges and calibration behavior on this hardware
- exact analog output usage and safe use cases for project
- exact relay contact arrangement and field wiring implications
- exact digital input electrical characteristics under field wiring
- actual free GPIO list after accounting for onboard peripherals

### 10.2 Firmware and Storage Unknowns

- whether OTA is currently configured in the existing firmware
- flash layout available for future web assets and logs
- whether current firmware uses EEPROM emulation, NVS, LittleFS, or SPIFFS

### 10.3 Networking Unknowns

- whether Ethernet is currently active on the connected device
- current IP address and service exposure
- whether any HTTP server is running now
- whether the existing firmware exposes telnet, MQTT, or web APIs

### 10.4 Safety-Critical Unknowns

- actual output state during brownout, reset, and watchdog reset
- whether relay boot chatter occurs beyond the observed self-test
- whether any field actuators are physically connected now
- whether the connected board is safe to reboot repeatedly during development

---

## 11. Recommended Immediate Next Hardware Actions

Before writing or flashing project firmware, the following hardware actions are recommended:

1. back up the existing flash from the board
2. read and store the current partition table
3. inspect whether the board responds over Ethernet or WiFi on the existing firmware
4. verify whether any relays are physically connected to external loads
5. verify safe behavior of every output with a meter or isolated test lamp before using real actuators
6. create a final pin and signal map based on measured evidence, not assumptions

---

## 12. Rules For Using This Hardware Profile

- treat this document as the board truth source unless contradicted by stronger evidence
- when a fact is measured on the live board, prefer it over generic board-family assumptions
- when a value is uncertain, mark it as unknown instead of guessing
- do not convert planning assumptions into wiring decisions without verification

---

## 13. Current Bottom Line

The connected hardware is a real and healthy KC868-A6-class ESP32 controller reachable over USB serial and flashable from the current MacBook environment.

The board is **not blank**. It currently runs a custom firmware unrelated to the LPG project and that firmware:

- initializes onboard I2C devices successfully
- confirms presence of both PCF8574 expanders, OLED, and RTC
- uses active-low relay logic
- toggles relays during boot self-test
- attempts WiFi auto-connect with saved credentials

This means the board is suitable for development, but only after the project treats it as an already-programmed controller and handles backup, safety isolation, and board-state verification properly.

---

## 14. Backup and Flash Layout Evidence

### 14.1 Flash Backup

A full flash backup was created from the connected board.

Backup details:

- backup date: `2026-04-07`
- source port: `/dev/cu.usbserial-110`
- flash size read: `4 MB`
- backup file: `backups/board-2026-04-07/flash-full-4mb.bin`
- SHA-256: `40a2f8e716273e02d071b42475ed8add546fae7a1049b775270e4effebb3d7f5`

This file is the current recovery baseline for the connected controller.

### 14.2 Partition Table

The partition table was extracted from the flash backup at the standard ESP32 partition table offset.

Observed partitions:

- `nvs` data/nvs at `0x009000`, size `0x005000`
- `otadata` data/ota at `0x00e000`, size `0x002000`
- `app0` app/ota_0 at `0x010000`, size `0x140000`
- `app1` app/ota_1 at `0x150000`, size `0x140000`
- `spiffs` data/spiffs at `0x290000`, size `0x160000`
- `coredump` data/coredump at `0x3f0000`, size `0x010000`

### 14.3 Interpretation

This connected board currently uses a dual-OTA style layout with:

- two application slots
- persistent NVS storage
- dedicated SPIFFS storage region
- dedicated coredump region

Implications for the LPG project:

- OTA-capable partitioning is feasible within `4 MB`
- a substantial filesystem region is available in the current layout model
- future firmware design must explicitly decide whether to retain a similar OTA layout or replace it with a project-specific scheme
- any flash replacement must preserve a recovery path and a documented partition strategy
