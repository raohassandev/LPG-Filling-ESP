# Load Cell and HX711 Connection Guide

## Purpose

This guide explains how to connect a single load cell through an `HX711` amplifier for the KC868-A6 LPG filling project.

It is written for the current verified hardware state of this project:

- KC868-A6 board confirmed
- onboard I2C already in use on `GPIO4` and `GPIO15`
- relay and input expanders already occupy the onboard I2C path
- free GPIO on the board is limited and must be verified before final pin freeze

Because of that, this document gives the correct electrical method and integration rules without pretending the final two ESP32 pins are already frozen.

## 1. Recommended Architecture

Use this signal chain:

`Load Cell -> HX711 -> ESP32 GPIO pair -> Firmware`

Recommended project rule:

- the load cell must never be connected directly to an ESP32 analog pin
- the load cell must always go through an instrumentation front end such as `HX711`
- the HX711 should be treated as a dedicated measurement path, separate from the relay and I2C expansion path

## 2. What the HX711 Does

The `HX711` is a 24-bit load-cell ADC module with:

- bridge excitation output for the load cell
- differential input pair for the strain gauge bridge
- digital clock line `SCK`
- digital data line `DOUT`

For this project, it is the correct first-stage interface between the load cell and the ESP32.

## 3. Load Cell Side Wiring

Most 4-wire load cells use these wires:

- `Red` = `E+` or excitation positive
- `Black` = `E-` or excitation negative
- `White` = `A-` or signal negative
- `Green` = `A+` or signal positive

Some manufacturers swap colors. Always verify with the actual load-cell datasheet or label before powering.

Typical connection from load cell to HX711:

- load cell `E+` -> HX711 `E+`
- load cell `E-` -> HX711 `E-`
- load cell `A+` / `S+` -> HX711 `A+`
- load cell `A-` / `S-` -> HX711 `A-`

If the load cell has a 6-wire cable:

- connect `sense+` and `sense-` according to the load cell manufacturer recommendation
- do not guess 6-wire mappings from color only

## 4. HX711 to KC868-A6 Wiring

### 4.1 Power

Use:

- HX711 `VCC` -> clean `3.3V` supply if the module supports `3.3V`
- HX711 `GND` -> KC868-A6 `GND`

Preferred rule for this project:

- run the HX711 digital side at `3.3V` logic when possible
- avoid `5V` logic on `DOUT` into ESP32 GPIO

If your HX711 module is only stable at `5V`, you must confirm the `DOUT` high level is safe for ESP32 input or add level protection.

### 4.2 Data Lines

Connect:

- HX711 `DOUT` / `DT` -> KC868-A6 `IO-1` / ESP32 `GPIO32`
- HX711 `SCK` -> KC868-A6 `IO-2` / ESP32 `GPIO33`

Important:

- do not use the verified onboard I2C pins `GPIO4` and `GPIO15` for HX711
- do not assume random spare pins from internet examples
- do not use UART TX/RX pins for HX711 unless the firmware is deliberately changed and serial-console side effects are understood
- do not use `GPIO12` for HX711 on ESP32 unless you have verified boot strapping behavior; it is safer to avoid it

### 4.3 Grounding

Required:

- HX711 ground and KC868-A6 ground must be common
- load-cell cable shield, if present, should be bonded according to the site grounding plan

Preferred practice:

- terminate shield at one side only unless the sensor/vendor documentation says otherwise
- keep the load-cell cable away from relay output wiring, pump contactor wiring, and solenoid cables

## 5. Mechanical Installation Rules

For meaningful readings:

- mount the load cell rigidly
- avoid twisting load paths
- ensure the platform transfers force vertically as intended by the load-cell design
- protect the scale frame from side loading and shock loading
- keep hose pull and cylinder handling from mechanically biasing the weight platform

In LPG service, mechanical errors will often dominate before firmware filtering does.

## 6. Noise and Stability Rules

The HX711 is sensitive to wiring quality and ground noise.

Use these rules:

- keep the load-cell signal cable short where possible
- route sensor cable separately from relay, valve, and motor wiring
- use twisted pairs for bridge wiring if extending cable
- avoid sharing noisy power rails with inductive loads
- add local decoupling close to the HX711 module

For early development:

- test with relays switching nearby
- compare idle reading drift with relays off vs on
- log zero drift before implementing final filtering logic

## 7. KC868-A6 Project Constraints

These constraints apply specifically to this repo and board:

- onboard I2C is already active on `GPIO4` and `GPIO15`
- detected onboard devices are at `0x22`, `0x24`, `0x3C`, and `0x68`
- the board appears to have only `2` free GPIO according to current documentation

Implication:

- a direct HX711 connection is possible only if those two free GPIO are confirmed and remain unused by final firmware features
- if those pins cannot be cleanly allocated, the safer fallback is an external weighing subsystem with a defined serial or industrial interface

## 8. Initial Bring-Up Procedure

Before integrating into fill logic:

1. Power the board with all hazardous outputs disconnected.
2. Wire the HX711 and load cell only.
3. Confirm the chosen `DOUT` and `SCK` pins do not conflict with onboard peripherals.
4. Read raw counts at rest for at least 2 minutes.
5. Apply known weights and confirm direction:
   - increasing load should increase the calibrated value
6. If direction is reversed:
   - swap `A+` and `A-` on the HX711 input side
7. Record zero drift, noise band, and settling time.
8. Only then add tare, calibration factor, and fill cut-off logic.

## 9. Calibration Workflow

Minimum calibration workflow:

1. Empty platform
2. record raw zero
3. place certified reference weight
4. record raw loaded value
5. compute scale factor
6. verify at multiple points if possible

Required project rule:

- calibration constants must be stored persistently
- calibration changes must be logged as maintenance events
- commercial metrology approval is a separate step from engineering calibration

## 10. Firmware Integration Notes

When the firmware implementation begins:

- use the active hardware-backed [WeightService.cpp](../firmware/lpg_controller/src/WeightService.cpp) as the HX711 integration point
- isolate raw ADC read, filtering, tare, and calibration from the fill-state machine
- expose raw counts, filtered weight, stable/unstable status, and calibration status separately

Do not:

- mix relay timing logic and weight sampling in one blocking loop
- bury calibration constants directly inside fill-controller code
- treat a noisy raw reading as fill-authoritative without stability checks

## 11. Open Items Before Final Wiring Freeze

These items still need verification on the actual board:

- verify `IO-1` / `GPIO32` and `IO-2` / `GPIO33` on the actual terminal/header layout before permanent wiring
- whether `3.3V` rail quality is sufficient for stable HX711 operation
- actual noise impact when relays and external loads are active

Until those are confirmed, this guide should be treated as the correct wiring method, not the final pin map.
