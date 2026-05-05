# Safety Policy — KC868-A6 LPG Controller

## THIS SYSTEM MUST NOT BE USED WITH LIVE LPG UNTIL ALL ITEMS BELOW ARE SATISFIED

---

## Firmware Safety Assumptions

The firmware enforces the following before any fill can start:

1. No active fault
2. Emergency stop circuit is closed (OK)
3. Cylinder detection input is active
4. Nozzle engagement input is active
5. HX711 scale is initialized and reading
6. Scale weight is stable (10-reading window)
7. Scale has been calibrated (calibrationValid flag)
8. Transaction logging is operational
9. Target weight > 0, rate > 0

During fill, the firmware monitors:

- E-stop: immediate fault + all outputs off
- Nozzle disengagement: immediate fault + all outputs off
- Scale read failure: immediate fault + all outputs off
- Overfill: net weight > target + 500 g → fault
- No-flow: weight delta < 10 g over any 10-second window → fault
- Fill timeout: 5 minutes from fill start → fault
- All faults close all relay outputs before transitioning to Fault state

## REQUIRED HARDWIRED E-STOP — HARDWARE VALIDATION REQUIRED

**The firmware E-stop alone is NOT sufficient for LPG safety.**

A hardwired, normally-closed E-stop circuit **must** cut power to all valve and pump relays independently of the ESP32. This ensures:
- MCU crash or firmware bug cannot leave valves open
- Network attack cannot bypass the physical cutoff
- Power-loss defaults to valves closed (fail-closed)

**This item requires hardware validation before live LPG use.**

## Valve/Pump Fail-Closed Requirement — HARDWARE VALIDATION REQUIRED

All LPG valves and the fill pump must be **normally-closed, energize-to-open** type:
- Relay off (de-energized) = valve closed
- Power loss = valve closed
- Firmware crash = valve closed

The relay bank is initialized with `writeAllSafe()` on every boot, which sets all outputs to the de-energized (safe) state before any other initialization runs.

**Verify with actual hardware that relays and valves fail closed before live LPG use.**

## HX711 / Load Cell — HARDWARE VALIDATION REQUIRED

- HX711 initialization failure sets a fault flag that blocks fill start
- Persistent read errors during fill trigger an immediate fault
- Scale calibration must be performed with certified reference weights
- Calibration validity is persisted across reboots
- **Verify calibration accuracy against certified reference weights before live LPG use**

## Input Truth Table — HARDWARE VALIDATION REQUIRED

| Input | GPIO | Expected Active State | Purpose |
|-------|------|-----------------------|---------|
| Cylinder detect | IN0 | HIGH when cylinder present | Block fill if absent |
| Nozzle engaged | IN1 | HIGH when engaged | Block fill if absent |
| E-stop | IN3 | LOW when OK (NC circuit) | Fault if opens |

**Wire inputs and verify truth table before live LPG use.**

## Simulation Mode Warning

If `LPG_DEV_BUILD` is defined, simulation endpoints are compiled in. Simulation mode overrides physical scale and input readings. **Never use simulation mode with real LPG connections.** The app shows a warning banner when `simActive=true` is reported in status.

## Dry-Test Requirements

Before any dry test (disconnected outputs):
- Verify relay initialization (all off at boot)
- Verify E-stop fault path closes all relays
- Verify fill start blocks under each gate condition
- Verify overfill/no-flow/timeout all fault correctly

## Live LPG Requirements

Live LPG use requires ALL of the following:
1. All software P0/P1 blockers resolved
2. Hardwired E-stop installed and tested
3. Fail-closed valves verified
4. Load cell and calibration certified
5. Input wiring verified against truth table
6. Regulatory/safety review by a qualified LPG engineer
7. Dry run completed with outputs physically disconnected

**Do not bypass this checklist.**
