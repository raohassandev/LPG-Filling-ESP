# LPG Controller — HMI Development Guide

**For:** Weintek / any Modbus TCP HMI panel developer  
**Device:** KC868-A6 LPG Filling Controller  
**Protocol:** Modbus TCP  
**Last updated:** 2026-05-14

---

## 1. Connection Settings

Enter these settings in your HMI communication configuration:

| Setting | Value |
|---------|-------|
| Protocol | Modbus TCP |
| Device IP Address | 192.168.0.100 *(check web page if different)* |
| Port | 502 |
| Unit ID / Station Number | 1 |
| Word Order | High Word First (Big Endian) |
| Float Format | IEEE 754, 32-bit, High Word First |

> **Note:** All addresses in this guide use **Weintek-style (40001+)** numbering.  
> Example: Weintek address **40001** = PDU register **0x0000**.

---

## 2. Two Types of Registers — Read vs Write

The controller has **two separate groups** of registers:

### Group A — Live Process Data (READ ONLY)
These show what is happening right now. You **cannot write** to these in production firmware.

| Weintek Address | Description | Type |
|----------------|-------------|------|
| 40001 – 40002 | Live Weight (kg) | Float32 |
| 40003 – 40004 | Tare Weight (kg) | Float32 |
| 40005 – 40006 | Net Weight (kg) | Float32 |
| 40007 – 40008 | Target Weight (kg) | Float32 |
| 40009 – 40010 | Rate Per Kg (PKR/kg) | Float32 |
| 40011 – 40012 | Target Amount (PKR) | Float32 |
| 40013 – 40014 | Current Amount (PKR) | Float32 |
| 40015 | Fill State | UINT16 |
| 40016 | E-Stop OK (1=OK) | UINT16 |
| 40017 | Cylinder Present (1=yes) | UINT16 |
| 40018 | Nozzle Engaged (1=yes) | UINT16 |
| 40019 | Weight Stable (1=yes) | UINT16 |

### Group B — HMI Command Block (READ + WRITE)
This is how the HMI **controls** the machine. Always use these for writing.

| Weintek Address | Name | Read/Write | Description |
|----------------|------|-----------|-------------|
| 40129 | CommandCode | **Write** | What command to run |
| 40130 | CommandSeq | **Write** | Trigger — increment each command |
| 40131 | LastAcceptedSeq | Read | Echo when command received |
| 40132 | CommandResult | Read | Result of last command |
| 40133 | CommandErrCode | Read | Error detail |
| 40134 | CommandBusy | Read | 1=busy, wait before next command |
| 40135 | FillMode | Read/Write | 0=By Weight, 1=By Amount |
| 40136 | PreparedFlag | Read | 1=ready to start |
| 40137 | ReadyToPrepare | Read | 1=safe to send PrepareNext |
| 40138 | ReadyToStart | Read | 1=safe to send Start |
| 40139 | CanTare | Read | 1=tare allowed now |
| 40140 | CanStop | Read | 1=fill active, stop allowed |
| 40141 | Heartbeat | **Write** | Write any value every 20 sec |
| 40142 | WatchdogTimeout (sec) | Read/Write | Default 30 sec (0=disabled) |
| 40143 | HeartbeatAgeSec | Read | Seconds since last heartbeat |
| 40145 – 40146 | Preset Tare (kg) | **Write** | Float32 — tare weight |
| 40147 – 40148 | Preset Target Kg | **Write** | Float32 — fill target kg |
| 40149 – 40150 | Preset Rate Per Kg | **Write** | Float32 — PKR per kg |
| 40151 – 40152 | Preset Target Amount | **Write** | Float32 — PKR amount |
| 40153 | PresetValid | Read | 1=preset accepted |
| 40154 | PresetErrCode | Read | Error from PrepareNext |
| 40155 | LastFillResult | Read | Result of last fill |
| 40156 | LastFillErrCode | Read | Error of last fill |

---

## 3. Command Codes

Write these to **40129 (CommandCode)**, then increment **40130 (CommandSeq)**:

| Code | Command | When to use |
|------|---------|------------|
| **10** | PrepareNext | After writing preset values — validate and prepare |
| **20** | AckComplete | After fill finishes — acknowledge and return to Idle |
| **21** | Tare (hardware) | Zero the scale with empty cylinder |
| **22** | Set Tare | Apply the preset tare value |
| **23** | Zero Net | Set tare = current live weight |
| **30** | Start Fill | Begin filling (only after PrepareNext succeeds) |
| **31** | Stop Fill | Emergency stop fill |
| **32** | Reset | Clear fault and return to Idle |
| **40** | Mode By-Kg | Switch to fill-by-weight mode |
| **41** | Mode By-Amount | Switch to fill-by-amount mode |

---

## 4. Command Result Codes

Read **40132 (CommandResult)** after sending a command:

| Value | Meaning | What to do |
|-------|---------|------------|
| 0 | Idle | No command processed yet |
| 1 | Accepted | Command received, running |
| 2 | Busy | Wait — previous command still running |
| 3 | **Rejected** | Command refused — read 40133 for reason |
| 4 | Done | Command completed successfully |
| 5 | Failed | Command failed — read 40133 for reason |

---

## 5. Error Codes (40133 and 40154)

| Code | Meaning | Solution |
|------|---------|---------|
| 0 | No error | — |
| 1 | Invalid preset | Check target kg > 0 and ≤ 500 |
| 2 | Busy | Wait and retry |
| 3 | Not prepared | Send PrepareNext first |
| 4 | Safety not ready | Check E-Stop, cylinder, nozzle |
| 5 | Weight unstable | Wait for scale to stabilise |
| 6 | **Watchdog timeout** | Send heartbeat to 40141 first |
| 7 | Invalid command | Wrong command code |
| 8 | Fault active | Send Reset (code 32) first |
| 9 | Scale not ready | Scale not initialised |
| 10 | Calibration invalid | Calibrate the scale |

---

## 6. Fill State Values (40015)

| Value | Label | Meaning |
|-------|-------|---------|
| 0 | IDLE | Machine waiting, ready for preset |
| 1 | READY | Conditions met, can start |
| 2 | VALIDATING | Checking conditions |
| 3 | FILLING FAST | Fast fill in progress |
| 4 | FILLING SLOW | Slow (fine) fill in progress |
| 5 | SETTLING | Weight stabilising after fill |
| 6 | COMPLETE | Fill finished successfully |
| 7 | ABORTED | Fill stopped by operator |
| 8 | FAULT | Error — needs reset |
| 9 | MAINTENANCE | Maintenance mode |

---

## 7. Screen-by-Screen Guide

### Screen 1 — Main Dashboard

**Display these values (read every 1–2 seconds):**

| What to show | Register | Type |
|-------------|----------|------|
| Live Weight | 40001 | Float32 |
| Net Weight | 40005 | Float32 |
| Target Weight | 40007 | Float32 |
| Fill State text | 40015 | UINT16 — show label from table above |
| E-Stop OK | 40016 | UINT16 — green/red indicator |
| Cylinder Present | 40017 | UINT16 — green/red indicator |
| Nozzle Engaged | 40018 | UINT16 — green/red indicator |
| Weight Stable | 40019 | UINT16 — green/red indicator |

**Buttons:**
- **START** → goes to Screen 2 (Preset Entry)
- **STOP** → send command 31
- **RESET** → send command 32
- **TARE** → send command 21 (only show if 40139=1)

---

### Screen 2 — Preset Entry (Before Fill)

**Operator enters:**
- Tare weight (empty cylinder kg) → write Float32 to **40145/40146**
- Target fill kg → write Float32 to **40147/40148**
- Rate per kg (PKR) → write Float32 to **40149/40150**

**Button: CONFIRM / PREPARE**
→ Send command **10** (PrepareNext)
→ Read 40136 (PreparedFlag) — wait for `1`
→ If 40132 = Rejected, show error from 40133
→ If PreparedFlag = 1, go to Screen 3

> **Tip:** Use a Float input box linked to address 40147 in Weintek — it automatically handles the 2-register Float32 split.

---

### Screen 3 — Ready to Fill

Show summary:
- Target: [value from 40147/40148]
- Tare: [value from 40145/40146]
- Rate: [value from 40149/40150]

**Button: START FILL**
→ Check **40138 (ReadyToStart) = 1** before enabling button
→ Send command **30** (Start)
→ Check 40132 = Accepted (1)
→ Go to Screen 4

**Button: BACK** → go to Screen 2

---

### Screen 4 — Filling In Progress

**Display live:**
- Live Weight (40001) — large number
- Net Weight (40005)
- Target Weight (40007)
- Fill State (40015) — show FAST / SLOW / SETTLING
- Current Amount PKR (40013)

Show a progress bar:
- Progress % = (Net Weight / Target Weight) × 100

**Button: EMERGENCY STOP**
→ Send command **31** (Stop)

When **40015 = 6** (Complete) → go to Screen 5

---

### Screen 5 — Fill Complete

Show results:
- Final Net Weight (40005)
- Total Amount PKR (40013)
- Fill Result (40155) — 4=Done, 5=Failed

**Button: OK / NEW FILL**
→ Send command **20** (AckComplete)
→ Go back to Screen 1 or Screen 2

---

## 8. Heartbeat — Very Important

The controller has a **watchdog timer** (default 30 seconds).  
If the HMI does not write to **40141** every 20 seconds, the controller will **block PrepareNext and Start**.

**Setup in Weintek:**
- Create a **Macro** or use a **Timer** component
- Every **15 seconds**, write value `1` to address **40141**
- This must run on every screen, always in background

> If you see **ErrCode = 6 (Watchdog Timeout)** — it means heartbeat was not sent. Send heartbeat first, then retry the command.

---

## 9. How to Send a Command — Step by Step

Every command follows this same pattern:

```
1. Write CommandCode value → address 40129
2. Write NEW sequence number → address 40130  (add 1 each time)
3. Wait 200–500 ms
4. Read CommandResult from 40132
5. If result = 3 (Rejected) or 5 (Failed) → read 40133 for error
```

**Important:** The sequence number (40130) must be **different from the last one**. The controller ignores commands with the same sequence number. Simply add 1 each time.

---

## 10. Complete Fill Sequence Summary

```
┌─────────────────────────────────────────────────┐
│  1. Send Heartbeat → 40141 = 1                  │
│  2. Write Tare     → 40145/40146 (Float32)       │
│  3. Write Target   → 40147/40148 (Float32)       │
│  4. Write Rate     → 40149/40150 (Float32)       │
│  5. CommandCode=10 → 40129                       │
│     CommandSeq+1   → 40130                       │
│  6. Wait → Read 40136 (PreparedFlag) = 1 ?       │
│     No  → Show error from 40133, stop            │
│     Yes → continue                               │
│  7. CommandCode=30 → 40129                       │
│     CommandSeq+1   → 40130                       │
│  8. Monitor 40015 (Fill State)                   │
│     Show live weight from 40001                  │
│  9. When 40015 = 6 (Complete):                   │
│     Show results from 40005, 40013               │
│     CommandCode=20 → 40129                       │
│     CommandSeq+1   → 40130                       │
│ 10. Back to start                                │
└─────────────────────────────────────────────────┘
```

---

## 11. Common Mistakes and Fixes

| Problem | Cause | Fix |
|---------|-------|-----|
| PrepareNext always Rejected, ErrCode=6 | Heartbeat not sent | Write any value to 40141 first |
| Target weight stays 0 on web page | PrepareNext not sent yet | Send command 10 first, then 30 |
| Write to 40007 has no effect | 40007 is read-only | Write to 40147/40148 instead |
| Command ignored | Same CommandSeq used twice | Always increment 40130 |
| Start Rejected, ErrCode=3 | PrepareNext not done | Send command 10 first |
| Start Rejected, ErrCode=4 | Safety inputs not ready | Check cylinder/nozzle/estop |
| Start Rejected, ErrCode=5 | Scale not stable | Wait 2–3 seconds and retry |
| Fill State stuck at FAULT | Error occurred | Send command 32 (Reset) |

---

## 12. Float32 Writing Tips (Weintek Specific)

- In Weintek EasyBuilder, set the address to **40147** with data type **Float**
- Weintek automatically writes both 40147 (Hi) and 40148 (Lo) together
- Do **not** write Hi and Lo separately — always use Float type so both words are written in one transaction (FC16)
- Same applies to 40145/40146, 40149/40150, 40151/40152

---

## 13. Minimal Register Poll List

To keep the HMI responsive, poll these registers in one FC03 read (read all at once):

**Fast poll — every 500 ms:**
Read 40001 to 40019 (19 registers) — live process data

**Medium poll — every 1 second:**
Read 40129 to 40143 (15 registers) — HMI command status

**Slow poll — every 5 seconds:**
Read 40155 to 40156 (2 registers) — last fill result

---

*End of guide. For firmware source and full register map, see `docs/user/MODBUS_PROTOCOL.md`.*
