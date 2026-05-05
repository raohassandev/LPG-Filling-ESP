# Dry Test Checklist — KC868-A6 LPG Controller

**IMPORTANT: All tests must be performed with valve/pump outputs PHYSICALLY DISCONNECTED or directed to safe dummy loads. No LPG or pressurized gas during dry testing.**

## Pre-Test Setup

- [ ] Flash firmware without `LPG_DEV_BUILD` defined
- [ ] `LPG_MODBUS_WRITES_ENABLED=0` confirmed (default)
- [ ] All relay outputs physically disconnected or wired to indicator lights only
- [ ] Calibrated load cell connected to HX711
- [ ] Cylinder-detect and nozzle-engage inputs wired to test switches
- [ ] E-stop wired to normally-closed test switch
- [ ] Serial monitor connected at 115200 baud

---

## 1. Relay Safe-Boot Test

**Procedure:** Power on. Immediately check relay outputs before any action.

- [ ] All 6 relay outputs are de-energized at boot (no click, no LED, multimeter confirms open)
- [ ] Serial log shows `[RELAY] All relays set safe`

---

## 2. E-Stop Test

**Procedure:** Open E-stop switch while device is in Idle state.

- [ ] State transitions to FAULT immediately
- [ ] All relay outputs de-energize
- [ ] reasonCode = "emergency_stop" in `/api/status`
- [ ] State stays FAULT until reset

**Procedure:** Attempt to start fill with E-stop open.

- [ ] Start returns error "Emergency stop is active"

---

## 3. Fill Start Block Tests

For each condition below: leave all others satisfied, break only the tested one, attempt start.

- [ ] **No cylinder** (open cylinder-detect input) → "Cylinder is not detected"
- [ ] **No nozzle** (open nozzle-engage input) → "Nozzle is not engaged"
- [ ] **Scale not initialized** (disconnect HX711) → "Scale not initialized — check HX711 wiring"
- [ ] **Scale unstable** (hold scale mid-reading) → "Scale not stable — wait for weight to settle"
- [ ] **Scale not calibrated** (fresh firmware, no cal performed) → "Scale not calibrated — calibrate before filling"
- [ ] **Active fault** (trigger E-stop, clear E-stop, do not reset) → "Reset fault before starting"
- [ ] **Fill already active** (start once, try start again) → "Fill is already active"

---

## 4. Normal Fill Cycle Test

**Procedure:** Satisfy all gate conditions. Start fill with a target weight reachable by the test load cell.

- [ ] State transitions: Idle → FILLING_FAST → FILLING_SLOW → SETTLING → COMPLETE
- [ ] Fast→Slow transition occurs at `slowFillThreshold * targetWeight`
- [ ] Relays activate on start, de-energize on SETTLING entry
- [ ] Transaction logged in `/api/transactions`
- [ ] Final net weight recorded in transaction

---

## 5. Overfill Test

**Procedure:** Use simulation mode (`LPG_DEV_BUILD`). Start fill. Advance simulated weight to `targetWeight + 0.6 kg`.

- [ ] State transitions to FAULT
- [ ] reasonCode = "overfill"
- [ ] All relay outputs de-energize

---

## 6. No-Flow Detection Test

**Procedure:** Start fill. Do not change weight. Wait > 10 seconds.

- [ ] State transitions to FAULT within ~10 seconds
- [ ] reasonCode = "no_flow"
- [ ] All relay outputs de-energize

---

## 7. Fill Timeout Test

**Procedure:** Start fill. Advance weight very slowly (just enough to avoid no-flow). Wait > 5 minutes.

- [ ] State transitions to FAULT at ~5 minutes
- [ ] reasonCode = "fill_timeout"
- [ ] All relay outputs de-energize

---

## 8. Nozzle Disengagement During Fill Test

**Procedure:** Start fill (all conditions met). Open nozzle-engage input mid-fill.

- [ ] State transitions to FAULT immediately
- [ ] reasonCode = "nozzle_disengaged"
- [ ] All relay outputs de-energize

---

## 9. Reboot During Fill Test

**Procedure:** Start fill. Pull power during FILLING_FAST state.

- [ ] On reboot: state = IDLE (not FILLING)
- [ ] All relay outputs de-energized at boot
- [ ] Transaction log shows incomplete/aborted transaction

---

## 10. API Auth Tests

For each endpoint listed below, attempt without `Authorization` header:

- [ ] `GET /api/status` → 401
- [ ] `GET /api/weight` → 401
- [ ] `GET /api/settings` → 401
- [ ] `GET /api/logs` → 401
- [ ] `GET /api/transactions.csv` → 401
- [ ] `GET /api/wifi` → 401
- [ ] `GET /api/network` → 401
- [ ] `POST /api/start` → 401
- [ ] `GET /api/health` → 200 (public)
- [ ] `GET /api/version` → 200 (public)

Role gate tests (use operator token):

- [ ] `GET /api/logs` with operator token → 403 (requires admin)
- [ ] `GET /api/transactions.csv` with operator token → 403 (requires admin)
- [ ] `POST /api/calibrate` with operator token → 403 (requires maintenance)

---

## 11. Modbus Write Rejection Test

With default firmware (`LPG_MODBUS_WRITES_ENABLED=0`):

- [ ] FC06 Write Single Register to kHR_Command (register 0x0018) → exception response
- [ ] FC16 Write Multiple Registers → exception response
- [ ] FC03 Read Holding Registers → success (reads are always enabled)

---

## 12. Calibration Audit Test

**Procedure:** Perform a calibration action via `/api/calibrate`. Read event log.

- [ ] `/api/logs` contains a `CAL` event with user, mode, old/new factor, and timestamp
- [ ] Two-point calibration: both point-set events logged

---

## 13. Transaction Log Failure Test

**Procedure:** Fill SPIFFS to capacity (or mock `startTransaction` to return 0).

- [ ] Fill start returns error "Transaction log failed — check storage"
- [ ] No fill starts without a valid transaction record

---

## Sign-Off

| Test | Pass | Tester | Date | Notes |
|------|------|--------|------|-------|
| Safe Boot | | | | |
| E-Stop | | | | |
| Fill Start Blocks | | | | |
| Normal Fill Cycle | | | | |
| Overfill | | | | |
| No-Flow | | | | |
| Timeout | | | | |
| Nozzle Disengage | | | | |
| Reboot During Fill | | | | |
| API Auth | | | | |
| Modbus Write Reject | | | | |
| Cal Audit | | | | |
| TxnLog Failure | | | | |

**All 13 test groups must pass before proceeding to hardware integration.**
