# Implementation Notes

## Current Direction

The project is proceeding with a custom ESP32 firmware baseline rather than relying on stock KinCony KCS behavior for the LPG control logic.

Reasoning:

- the connected board is already running unrelated custom firmware
- KCS assumptions are not strong enough for deterministic LPG control
- the board resources are now sufficiently known to start custom firmware scaffolding

## Current Firmware Baseline Includes

- board config for verified KC868-A6 resources
- active-low relay abstraction
- input expander abstraction
- status model
- fill controller skeleton
- serial command path for hardware-first testing
- preferences-backed settings load path
- local event log stored on SPIFFS
- device-hosted web portal using built-in ESP32 `WebServer`
- SPIFFS-hosted frontend

## Explicit Constraints

- no boot-time relay test sequences
- no network dependency for core control logic
- no unverified pin assumptions beyond the confirmed I2C bus and expander addresses
- no claim of production readiness until relay/input truth tables are tested on hardware
- no final HX711 pin freeze until the actual free GPIO exposure is verified on the KC868-A6

## Hardware References

- load cell and HX711 wiring guidance is documented in [load_cell_hx711_connection_guide.md](/Users/israrulhaq/Desktop/DEV/LPG-Filling-ESP/docs/load_cell_hx711_connection_guide.md)

## Build Tooling Direction

- use the repo-local `arduino-cli-0.35.3` when available
- avoid the IDE-bundled `arduino-cli 1.2.0` with ESP32 core `2.0.0`, because that combination rejects the old ESP32 board options
- keep the build flow script-driven so compile and upload use the same toolchain path
