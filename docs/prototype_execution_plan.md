# LPG Filling Station Prototype Execution Plan

## Purpose

This document turns `LPG_Filling_Station_Development_Plan.md` into an implementation checklist for the current KC868-A6 firmware.

The immediate goal is a minimal, demonstrable prototype. Full cloud, multi-site management, and mobile app work should begin only after the prototype is accepted on real hardware.

## Phase 1 Scope: Minimal Prototype

### Control

- `FillController` owns the fill state machine.
- `RelayBank` owns active-low relay writes through the KC868-A6 relay expander.
- Fast fill uses Relay 1 and Relay 3.
- Slow fill uses Relay 2 and Relay 3.
- Stop, complete, abort, and fault force all relays off.

### Sensors

- `InputExpander` reads nozzle, cylinder, and E-stop states.
- `WeightService` reads HX711 on KC868-A6 `IO-1/GPIO32` and `IO-2/GPIO33`.
- Operators enter empty-cylinder tare weight separately from scale tare.
- Fill control and pricing use net weight: `net = live - tare`.
- Operators can zero net weight by setting tare equal to the current live scale reading.
- Calibration is not yet production-ready; the next implementation step is persistent calibration.

### Android App and API

- `WebPortal` serves API endpoints and a lightweight diagnostics landing page from SPIFFS.
- The operator, admin, and manufacturer UI lives in `apps/lpg-expo-app`.
- Operator role: tare, zero net, start/stop/reset, readiness, live/tare/net/target/current amount.
- Operator role supports weight-based and amount-based fill entry.
- Admin role: rate setup, transactions, sales totals, period filters.
- Manufacturer role: relay diagnostics and raw device status.
- The Expo app attempts WebSocket updates at `/ws` and falls back to `/api/status` polling.

### Modbus/HMI Contract

- Active display/HMI integration uses Modbus RTU over RS485.
- Active register addresses are 0-based and live in `firmware/lpg_controller/include/ModbusRegisterMap.h`.
- Live, tare, net, target, rate, amount, state, readiness, RTC, stats, and alarm diagnostics are exposed in the `0x0000..0x0050` range.
- The old `0x1001..0x1006` TCP prototype map is legacy reference only and must not be used for current display firmware.
- The firmware also serves optional Modbus TCP on port `502`.

### Logging and History

- `EventLog` records runtime events in SPIFFS.
- `TransactionLog` stores each transaction as its own SPIFFS record file so updates can be rewritten safely.
- Transaction logging must never block filling; failed logging is a warning, not a process stop.

## Prototype Acceptance Checklist

1. Firmware compiles with `scripts/build_firmware.ps1`.
2. Firmware uploads to the KC868-A6 with `scripts/upload_firmware.ps1`.
3. SPIFFS UI uploads with `scripts/upload_spiffs.ps1`.
4. OLED shows STA IP, AP IP, and state.
5. Web UI loads from STA IP and fallback AP.
6. HX711 initializes and `tare` brings empty platform near zero.
7. Nozzle, cylinder, and E-stop UI tiles match live input states.
8. Operator tare weight updates net weight and current amount.
9. Zero Net sets tare to the current live weight and brings net weight to zero.
10. Weight mode calculates amount as `targetWeight * ratePerKg`.
11. Amount mode calculates target weight as `targetAmount / ratePerKg`.
12. Start moves state to `FILLING_FAST` and energizes Relay 1 + Relay 3.
13. Slow-fill threshold moves state to `FILLING_SLOW` and energizes Relay 2 + Relay 3.
14. Stop/reset/fault de-energize all relays.
15. A completed or aborted fill appears in `/api/transactions` with tare, net kg, rate, and final amount.
16. Admin can save rate per kg and the operator UI uses that rate.
17. `/api/modbus` returns the current Modbus register map values.

## Phase 1 Automated Checks

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test_prototype_contracts.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\build_firmware.ps1
```

The contract test script verifies source-level assumptions that do not require hardware.

## Phase 2 Backlog

- Persistent calibration workflow and audit log.
- Modbus TCP server and RS485/RTU transport binding after HMI hardware selection.
- Manager/owner authentication enforcement.
- Daily shift reports and export package.
- MQTT telemetry and REST cloud sync.
- Multi-dispenser/station identifiers.
- Cloud-side reconciliation and offline queue.
- Regulatory reports and tamper-evident audit trail.
