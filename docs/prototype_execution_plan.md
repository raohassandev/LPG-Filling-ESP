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
- Calibration is not yet production-ready; the next implementation step is persistent calibration.

### Web UI

- `WebPortal` serves a lightweight role-based console from SPIFFS.
- User role: start/stop/reset and readiness.
- User role displays live, tare, net, target, and current amount.
- Admin role: rate setup, transactions, sales totals, audit log.
- Manufacturer role: relay diagnostics and raw device status.

### Modbus/HMI Contract

- Holding register values are exposed as kg x 100 where applicable.
- `0x1001`: live weight.
- `0x1002`: tare weight.
- `0x1003`: net weight.
- `0x1004`: filling status code.
- `0x1005`: target weight.
- `0x1006`: E-stop status.
- The firmware contains a compact register map so TCP/RTU transport can be connected without changing business logic.

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
9. Start moves state to `FILLING_FAST` and energizes Relay 1 + Relay 3.
10. Slow-fill threshold moves state to `FILLING_SLOW` and energizes Relay 2 + Relay 3.
11. Stop/reset/fault de-energize all relays.
12. A completed or aborted fill appears in `/api/transactions` with tare, net kg, rate, and final amount.
13. Admin can save rate per kg and the operator UI uses that rate.
14. `/api/modbus` returns the current Modbus register map values.

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
