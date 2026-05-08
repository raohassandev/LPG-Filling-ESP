# LPG Filling Station Controller — Modbus TCP/RTU Protocol Reference

**Hardware:** KC868-A6 (ESP32)  
**Firmware:** v0.1.0  
**Document version:** 2.0  

---

## Table of Contents

1. [Connection Parameters — TCP](#1-connection-parameters)
2. [Connection Parameters — RTU (RS-485)](#2-modbus-rtu-rs-485)
3. [Frame Structure](#3-frame-structure)
4. [Supported Function Codes](#4-supported-function-codes)
5. [Data Types](#5-data-types)
6. [Holding Registers — Complete Map](#6-holding-registers)
7. [Coils — FC01/FC05](#7-coils)
8. [Discrete Inputs — FC02](#8-discrete-inputs)
9. [Exception Codes](#9-exception-codes)
10. [Fill State Enum Values](#10-fill-state-enum-values)
11. [Common Operation Sequences](#11-common-operation-sequences)
12. [Modbus Poll / ModScan Configuration](#12-modbus-poll--modscan-configuration)

---

## 1. Connection Parameters — TCP

| Parameter | Value |
|-----------|-------|
| Protocol | Modbus TCP |
| Port | 502 |
| Unit ID | Any (1 recommended, all values accepted) |
| Protocol ID | 0x0000 (standard Modbus) |
| Max frame size | 260 bytes |
| Connection model | Single persistent client (one active connection at a time) |
| TCP | No-delay enabled |

---

## 2. Modbus RTU (RS-485)

| Parameter | Value |
|-----------|-------|
| Interface | RS-485 half-duplex via UART2 |
| Default baud rate | 9600 |
| Default slave address | 1 |
| Default parity | None (8N1) |
| Default stop bits | 1 |
| DE/RE pin | Not used; board RS485 is auto-direction |
| UART2 RX | GPIO 14 |
| UART2 TX | GPIO 27 |
| Supported FCs | FC01, FC02, FC03, FC04, FC05, FC06, FC16 |
| Register map | Active 0-based holding-register map; see section 6 |

> **IMPORTANT:** Current working wiring uses the KC868-A6 RS485 port on GPIO27 TX and GPIO14 RX, not the RS232 pins GPIO16/GPIO17. Configure via `/api/modbus-rtu` or Modbus registers `0x0019..0x001D`. RTU is enabled by default on the current firmware.

### RTU Frame Format

```
┌────────┬────┬──────────────┬──────────┐
│ Addr 1 │ FC │ Data (N bytes)│ CRC16 LE 2│
└────────┴────┴──────────────┴──────────┘
```

CRC16 polynomial: `0xA001` (Modbus standard). Frame end is detected by an inter-character silence ≥ 5 ms.

---

## 2. Frame Structure

### Request / Response format

```
┌─────────────────────── MBAP Header (7 bytes) ──────────────────────────┐
│ Transaction ID (2) │ Protocol ID (2) │ Length (2) │ Unit ID (1) │ PDU… │
└─────────────────────────────────────────────────────────────────────────┘
```

| Field | Bytes | Description |
|-------|-------|-------------|
| Transaction ID | 2 | Echoed unchanged in response |
| Protocol ID | 2 | Must be `0x0000` — Modbus TCP |
| Length | 2 | Number of bytes following (Unit ID + PDU) |
| Unit ID | 1 | Echoed unchanged |
| PDU | 1+ | Function Code + Data |

**Minimum valid request size:** 8 bytes (7 MBAP + 1 FC byte).

---

## 3. Supported Function Codes

| Code | Name | Direction | Notes |
|------|------|-----------|-------|
| **FC01** | Read Coils | R | Reads relay states and boolean status flags |
| **FC02** | Read Discrete Inputs | R | Reads physical digital inputs 1–6 |
| **FC03** | Read Holding Registers | R | Reads any register (primary read function) |
| **FC04** | Read Input Registers | R | Alias for FC03 — same data, same address space |
| **FC05** | Write Single Coil | W | Writes individual relay outputs |
| **FC06** | Write Single Register | W | Writes UINT16 registers only |
| **FC16** | Write Multiple Registers | W | Required for FLOAT32 and UINT32 writes |

**Not supported:** FC08 (diagnostics), FC11 (event counter), FC15 (write coils), FC22/23 (mask/read-write). These return exception 0x01 (Illegal Function).

---

## 4. Data Types

### FLOAT32 (IEEE 754, big-endian, Hi word first)

Two consecutive 16-bit registers. The **Hi word** (most significant 16 bits) is at the lower address.

```
Register N   → bits 31..16 (exponent + part of mantissa)
Register N+1 → bits 15..0  (remainder of mantissa)
```

**Example: 12.5 kg = 0x41480000**

| Address | Value | Meaning |
|---------|-------|---------|
| 0x0000 | 0x4148 | Hi word |
| 0x0001 | 0x0000 | Lo word |

To write a FLOAT32 value, always use **FC16** (Write Multiple Registers) with 2 registers.  
You can read a FLOAT32 using **FC03** — request 2 registers starting at the Hi word address.

### UINT32 (big-endian, Hi word first)

Same layout as FLOAT32 — 2 registers, Hi word at lower address.

### UINT16

Single 16-bit register. Read with FC03, write with FC06 or FC16.

---

## 6. Holding Registers — Complete Map

### Address notation

PDU addresses are **0x0000-based**. In Modbus Poll "4-digit" mode the display offset is +40001, so PDU 0x0000 → 40001.

**`kHR_Base = 0x0000`; active map extends through diagnostic register `0x0050`.**

---

### Section 1 — Process Data (0x0000–0x0018, 25 registers)

| MP (40001+) | PDU | Name | Type | R/W | Unit | Description |
|-------------|-----|------|------|-----|------|-------------|
| 40001 | 0x0000 | Live Weight Hi | FLOAT32 Hi | R | kg | Current scale reading |
| 40002 | 0x0001 | Live Weight Lo | FLOAT32 Lo | R | kg | — |
| 40003 | 0x0002 | Tare Weight Hi | FLOAT32 Hi | R/W | kg | Empty cylinder weight |
| 40004 | 0x0003 | Tare Weight Lo | FLOAT32 Lo | R/W | kg | — |
| 40005 | 0x0004 | Net Weight Hi | FLOAT32 Hi | R | kg | Live − Tare |
| 40006 | 0x0005 | Net Weight Lo | FLOAT32 Lo | R | kg | — |
| 40007 | 0x0006 | Target Weight Hi | FLOAT32 Hi | R/W | kg | Fill target |
| 40008 | 0x0007 | Target Weight Lo | FLOAT32 Lo | R/W | kg | — |
| 40009 | 0x0008 | Rate/kg Hi | FLOAT32 Hi | R/W | PKR/kg | Price per kilogram |
| 40010 | 0x0009 | Rate/kg Lo | FLOAT32 Lo | R/W | PKR/kg | — |
| 40011 | 0x000A | Target Amount Hi | FLOAT32 Hi | R/W | PKR | Expected charge |
| 40012 | 0x000B | Target Amount Lo | FLOAT32 Lo | R/W | PKR | — |
| 40013 | 0x000C | Current Amount Hi | FLOAT32 Hi | R | PKR | Running charge (Net × Rate) |
| 40014 | 0x000D | Current Amount Lo | FLOAT32 Lo | R | PKR | — |
| 40015 | 0x000E | Fill State | UINT16 | R | — | See §10 |
| 40016 | 0x000F | E-Stop OK | UINT16 | R | — | 1=OK 0=tripped |
| 40017 | 0x0010 | Cylinder Present | UINT16 | R | — | 1=on scale |
| 40018 | 0x0011 | Nozzle Engaged | UINT16 | R | — | 1=connected |
| 40019 | 0x0012 | Weight Stable | UINT16 | R | — | 1=stable |
| 40020 | 0x0013 | Transaction Count Hi | UINT32 Hi | R | count | Total transactions ever |
| 40021 | 0x0014 | Transaction Count Lo | UINT32 Lo | R | count | — |
| 40022 | 0x0015 | Uptime Hi | UINT32 Hi | R | sec | Seconds since boot |
| 40023 | 0x0016 | Uptime Lo | UINT32 Lo | R | sec | — |
| 40024 | 0x0017 | Command | UINT16 | W | — | See §6.2. Reads 0. |
| 40025 | 0x0018 | Device ID | UINT16 | R | — | `0xA601` |

---

### Section 2 — Communications (0x0019–0x001F, 7 registers)

| MP (40001+) | PDU | Name | Type | R/W | Description |
|-------------|-----|------|------|-----|-------------|
| 40026 | 0x0019 | RTU Slave Address | UINT16 | R/W | 1–247. Default 1. |
| 40027 | 0x001A | RTU Baud Rate Hi | UINT32 Hi | R/W | e.g. 9600, 19200, 38400, 57600, 115200 |
| 40028 | 0x001B | RTU Baud Rate Lo | UINT32 Lo | R/W | — |
| 40029 | 0x001C | RTU Parity | UINT16 | R/W | 0=None 1=Even 2=Odd |
| 40030 | 0x001D | RTU Stop Bits | UINT16 | R/W | 1 or 2 |
| 40031 | 0x001E | TCP Port | UINT16 | R | Always 502 |
| 40032 | 0x001F | MQTT Connected | UINT16 | R | 0=disconnected 1=connected |

> Writing RTU parameters persists to flash. Changes take effect on next restart.

---

### Section 3 — RTC / Clock (0x0020–0x0027, 8 registers)

| MP (40001+) | PDU | Name | Type | R/W | Description |
|-------------|-----|------|------|-----|-------------|
| 40033 | 0x0020 | RTC Year | UINT16 | R/W | e.g. 2025 |
| 40034 | 0x0021 | RTC Month | UINT16 | R/W | 1–12 |
| 40035 | 0x0022 | RTC Day | UINT16 | R/W | 1–31 |
| 40036 | 0x0023 | RTC Hour | UINT16 | R/W | 0–23 |
| 40037 | 0x0024 | RTC Minute | UINT16 | R/W | 0–59 |
| 40038 | 0x0025 | RTC Second | UINT16 | R/W | 0–59 **† write triggers set** |
| 40039 | 0x0026 | Unix Timestamp Hi | UINT32 Hi | R/W | Seconds since 1970-01-01 UTC |
| 40040 | 0x0027 | Unix Timestamp Lo | UINT32 Lo | R/W | **write Lo triggers set** |

**Writing the clock:**
- Option A (field-by-field): Write Year → Month → Day → Hour → Minute → **Second last**. Writing Second commits all accumulated fields.
- Option B (atomic): Write UnixHi → **UnixLo**. Writing Lo commits the full timestamp.

---

### Section 4 — All-Time Statistics (0x0028–0x002F, 8 registers)

| MP (40001+) | PDU | Name | Type | R | Description |
|-------------|-----|------|------|---|-------------|
| 40041 | 0x0028 | All Completed Hi | UINT32 Hi | R | Total completed fills ever |
| 40042 | 0x0029 | All Completed Lo | UINT32 Lo | R | — |
| 40043 | 0x002A | All Failed Hi | UINT32 Hi | R | Total aborted + faulted |
| 40044 | 0x002B | All Failed Lo | UINT32 Lo | R | — |
| 40045 | 0x002C | All Kg Sold Hi | FLOAT32 Hi | R | Total LPG dispensed (kg) |
| 40046 | 0x002D | All Kg Sold Lo | FLOAT32 Lo | R | — |
| 40047 | 0x002E | All Amount Hi | FLOAT32 Hi | R | Total revenue (PKR) |
| 40048 | 0x002F | All Amount Lo | FLOAT32 Lo | R | — |

---

### Section 5a — Today Statistics (0x0030–0x0035, 6 registers)

| MP (40001+) | PDU | Name | Type | Description |
|-------------|-----|------|------|-------------|
| 40049 | 0x0030 | Today Completed | UINT16 | R | Completed fills today |
| 40050 | 0x0031 | Today Failed | UINT16 | R | Failed (abort+fault) today |
| 40051 | 0x0032 | Today Kg Hi | FLOAT32 Hi | R | kg dispensed today |
| 40052 | 0x0033 | Today Kg Lo | FLOAT32 Lo | R | — |
| 40053 | 0x0034 | Today Amount Hi | FLOAT32 Hi | R | Revenue today (PKR) |
| 40054 | 0x0035 | Today Amount Lo | FLOAT32 Lo | R | — |

---

### Section 5b — This Week (0x0036–0x003B)

| MP | PDU | Name | Type |
|----|-----|------|------|
| 40055 | 0x0036 | Week Completed | UINT16 |
| 40056 | 0x0037 | Week Failed | UINT16 |
| 40057 | 0x0038 | Week Kg Hi | FLOAT32 Hi |
| 40058 | 0x0039 | Week Kg Lo | FLOAT32 Lo |
| 40059 | 0x003A | Week Amount Hi | FLOAT32 Hi |
| 40060 | 0x003B | Week Amount Lo | FLOAT32 Lo |

---

### Section 5c — This Month (0x003C–0x0041)

| MP | PDU | Name | Type |
|----|-----|------|------|
| 40061 | 0x003C | Month Completed | UINT16 |
| 40062 | 0x003D | Month Failed | UINT16 |
| 40063 | 0x003E | Month Kg Hi | FLOAT32 Hi |
| 40064 | 0x003F | Month Kg Lo | FLOAT32 Lo |
| 40065 | 0x0040 | Month Amount Hi | FLOAT32 Hi |
| 40066 | 0x0041 | Month Amount Lo | FLOAT32 Lo |

---

### Section 5d — This Year (0x0042–0x0047)

| MP | PDU | Name | Type |
|----|-----|------|------|
| 40067 | 0x0042 | Year Completed | UINT16 |
| 40068 | 0x0043 | Year Failed | UINT16 |
| 40069 | 0x0044 | Year Kg Hi | FLOAT32 Hi |
| 40070 | 0x0045 | Year Kg Lo | FLOAT32 Lo |
| 40071 | 0x0046 | Year Amount Hi | FLOAT32 Hi |
| 40072 | 0x0047 | Year Amount Lo | FLOAT32 Lo |

> Statistics registers are read-only. Period stats are cached with a 30-second TTL; cache is invalidated immediately when any transaction completes, aborts, or faults. Period boundaries (today/week/month/year) are computed from the on-board RTC time.

---

### Section 5e - Diagnostics and Alarms (0x0048-0x0050)

| MP | PDU | Name | Type | Description |
|----|-----|------|------|-------------|
| 40073 | 0x0048 | Alarm Code | UINT16 | See alarm-code table below |
| 40074 | 0x0049 | Alarm Severity | UINT16 | 0=None, 1=Info, 2=Warning, 3=Alarm/Fault |
| 40075 | 0x004A | Readiness Mask | UINT16 | bit0 E-stop OK, bit1 Cylinder, bit2 Nozzle, bit3 Stable, bit4 Calibrated, bit5 Scale ready |
| 40076 | 0x004B | Blocker Mask | UINT16 | bit0 Fault, bit1 E-stop, bit2 Cylinder, bit3 Nozzle, bit4 Scale init, bit5 Scale read, bit6 Stable, bit7 Calibration, bit8 Simulation |
| 40077 | 0x004C | Scale Initialized | UINT16 | 1=HX711 initialized |
| 40078 | 0x004D | Scale Read Error | UINT16 | 1=Scale read failed |
| 40079 | 0x004E | Calibration Valid | UINT16 | 1=Calibration factor is valid |
| 40080 | 0x004F | Simulation Active | UINT16 | 1=Simulated weight mode active |
| 40081 | 0x0050 | Alarm Source | UINT16 | 0=None, 1=Safety, 2=Scale, 3=Process, 4=Operator/Comms |

| Code | Alarm | Severity | Meaning |
|------|-------|----------|---------|
| 0 | None | 0 | No active alarm |
| 1 | Emergency stop active | 3 | Emergency push button is pressed |
| 2 | Nozzle disengaged | 2 | Nozzle lock/engage input is not active |
| 3 | Cylinder missing | 2 | Cylinder-present input is not active |
| 4 | Scale read error | 3 | HX711/load cell read failed |
| 5 | Scale not stable | 2 | Weight must settle before START |
| 6 | Scale not calibrated | 2 | Calibration factor is missing or invalid |
| 7 | Overfill | 3 | Fill exceeded target/safety limit |
| 8 | Fill timeout | 3 | Fill ran beyond allowed time |
| 9 | No flow | 3 | Fill active but weight did not increase |
| 10 | Transaction log fault | 3 | Controller could not record transaction |
| 11 | Operator stop | 3 | Stop command came from operator, serial, or Modbus |
| 12 | Active controller fault | 3 | Fault state without a more specific reason |
| 13 | Controller offline | 3 | Display-local alarm when controller polling fails |

---

### 6.2 Command Register (40024 / 0x0017)

Write a command value using FC06. The register always reads back 0.

| Value | Command | Notes |
|-------|---------|-------|
| `1` | **Start Fill** | Uses Target Weight, Rate/kg, Target Amount already in registers |
| `2` | **Stop Fill** | Aborts current fill (reason: `modbus_stop`) |
| `3` | **Reset** | Resets Aborted/Fault → Idle |
| `4` | **Zero Net** | Sets Tare Weight = current Live Weight |

Any other value → Exception 0x03 (Illegal Data Value).

---

### 6.3 Writable Register Validation

| Register | Write constraint |
|----------|-----------------|
| Tare Weight | 0.0 ≤ value ≤ 500.0 kg |
| Target Weight | 0.0 ≤ value ≤ 500.0 kg |
| Rate/kg | 0.0 < value ≤ 100,000.0 PKR/kg |
| Target Amount | value ≥ 0.0 PKR |
| Command | 1–4 only |
| RTU Slave Address | 1–247 |
| RTU Baud Rate | 1200/2400/4800/9600/19200/38400/57600/115200 |
| RTU Parity | 0/1/2 |
| RTU Stop Bits | 1 or 2 |
| RTC Year | ≥ 2020 |
| RTC Month | 1–12 |
| RTC Day | 1–31 |
| RTC Hour | 0–23 |
| RTC Minute/Second | 0–59 |
| Unix Timestamp | ≥ 1577836800 (2020-01-01) |

Values outside these ranges return Exception 0x03 (Illegal Data Value).

---

## 6. Coils

**FC01 (Read Coils), FC05 (Write Single Coil)**  
**`kCoil_Base = 0x0000`, Count = 11 coils (0x0000–0x000A)**

In Modbus Poll, coils display as `00001 + address`.

| Modbus Poll (00001+) | PDU Address | Name | R/W | Description |
|---------------------|-------------|------|-----|-------------|
| 00001 | 0x0000 | E-Stop OK | R | 1 = E-Stop circuit intact |
| 00002 | 0x0001 | Cylinder Present | R | 1 = cylinder detected on scale |
| 00003 | 0x0002 | Nozzle Engaged | R | 1 = nozzle sensor active |
| 00004 | 0x0003 | Weight Stable | R | 1 = scale reading is stable |
| 00005 | 0x0004 | Fill Active | R | 1 = any fill state active (Fast/Slow/Settling) |
| 00006 | 0x0005 | Relay 1 | R/W | 1 = relay energized |
| 00007 | 0x0006 | Relay 2 | R/W | 1 = relay energized |
| 00008 | 0x0007 | Relay 3 | R/W | 1 = relay energized |
| 00009 | 0x0008 | Relay 4 | R/W | 1 = relay energized |
| 00010 | 0x0009 | Relay 5 | R/W | 1 = relay energized |
| 00011 | 0x000A | Relay 6 | R/W | 1 = relay energized |

**Write coil value encoding (FC05):**
- `0xFF00` = coil ON
- `0x0000` = coil OFF
- Any other value → Exception 0x03

Status coils 00001–00005 are **read-only**. Writing them returns Exception 0x02 (Illegal Data Address).

---

## 7. Discrete Inputs

**FC02 (Read Discrete Inputs)**  
**`kDI_Base = 0x0000`, Count = 6 inputs (0x0000–0x0005)**

In Modbus Poll, discrete inputs display as `10001 + address`.

| Modbus Poll (10001+) | PDU Address | Name | Description |
|---------------------|-------------|------|-------------|
| 10001 | 0x0000 | Input 1 | Cylinder-present sensor in current prototype wiring |
| 10002 | 0x0001 | Input 2 | Nozzle lock/engage sensor in current prototype wiring |
| 10003 | 0x0002 | Input 3 | Spare / cabinet door if wired |
| 10004 | 0x0003 | Input 4 | E-stop raw input; true means tripped, controller exposes `emergencyStopOk = false` |
| 10005 | 0x0004 | Input 5 | Physical digital input 5 (spare) |
| 10006 | 0x0005 | Input 6 | Physical digital input 6 (spare) |

All discrete inputs are **read-only**. FC02 only; no write function.

---

## 8. Exception Codes

| Code | Name | Meaning |
|------|------|---------|
| `0x01` | Illegal Function | Function code not supported |
| `0x02` | Illegal Data Address | Address out of range, or writing a read-only location |
| `0x03` | Illegal Data Value | Value outside allowed range, or invalid command code |

Exception response format: `FC | 0x80` (e.g., FC03 exception = `0x83`).

---

## 9. Fill State Enum Values

Register 40015 (Fill State) uses these values:

| Value | Name | Description |
|-------|------|-------------|
| 0 | IDLE | No fill in progress |
| 1 | READY | Parameters set, awaiting start |
| 2 | VALIDATING | Pre-fill sensor checks |
| 3 | FILLING_FAST | Fast solenoid open |
| 4 | FILLING_SLOW | Slow solenoid, final approach |
| 5 | SETTLING | Solenoids closed, weight stabilizing |
| 6 | COMPLETE | Fill finished, transaction saved |
| 7 | ABORTED | Stopped by operator or safety |
| 8 | FAULT | Hardware/sensor error |
| 9 | MAINTENANCE | Hardware test mode |

---

## 10. Common Operation Sequences

### 10.1 Read Current Status (FC03, all 25 registers)

```
Request: FC=0x03  StartAddr=0x0000  Qty=0x0019
Response: 25 registers × 2 bytes = 50 bytes of data
```

Decode in order:
- Regs 0–1 → Live Weight (FLOAT32)
- Regs 2–3 → Tare Weight (FLOAT32)
- Regs 4–5 → Net Weight (FLOAT32)
- ...
- Reg 14 → Fill State (UINT16)

---

### 10.2 Start a Fill Sequence (FC16 + FC06)

**Step 1 — Write Target Weight (12.5 kg)**

```
FC=0x10 (FC16)
StartAddr = 0x0006  (Target Weight Hi)
Qty       = 2
Data:
  0x4148 0x0000   ← IEEE 754 for 12.5f
```

**Step 2 — Write Rate/kg (250.0 PKR)**

```
FC=0x10 (FC16)
StartAddr = 0x0008  (Rate/kg Hi)
Qty       = 2
Data:
  0x4378 0x0000   ← IEEE 754 for 250.0f
```

**Step 3 — Write Target Amount (3125.0 PKR)**

```
FC=0x10 (FC16)
StartAddr = 0x000A  (Target Amount Hi)
Qty       = 2
Data:
  0x4543 0xD000   ← IEEE 754 for 3125.0f
```

**Step 4 — Send Start Command (FC06)**

```
FC=0x06 (FC06)
Addr  = 0x0017  (Command)
Value = 0x0001  (Start)
```

---

### 10.3 Bulk Write (Steps 1–3 in One FC16 Frame)

Write registers 0x0006–0x000B (Target Weight + Rate + Target Amount, 6 registers):

```
FC=0x10  StartAddr=0x0006  Qty=6
Data (12 bytes):
  [Hi_TargetWeight] [Lo_TargetWeight]
  [Hi_Rate]         [Lo_Rate]
  [Hi_TargetAmount] [Lo_TargetAmount]
```

Then send Start command as above.

---

### 10.4 Stop Fill (FC06)

```
FC=0x06  Addr=0x0017  Value=0x0002
```

---

### 10.5 Zero Net Weight (FC06)

```
FC=0x06  Addr=0x0017  Value=0x0004
```

Sets Tare = current Live Weight so Net = 0 kg.

---

### 10.6 Read Transaction Count (FC03, UINT32)

```
Request: FC=0x03  StartAddr=0x0013  Qty=0x0002
Response: 2 registers
  count = (reg[0x0013] << 16) | reg[0x0014]
```

---

### 10.7 Monitor Fill Progress (polling loop)

Poll these 4 registers every 500 ms:

```
FC=0x03  StartAddr=0x0004  Qty=0x000C
Reads: Net Weight (2) + Target Weight (2) + Rate (2) + Target Amount (2) + Current Amount (2) + Fill State (1) + E-Stop (1)
= 12 registers
```

---

### 10.8 Read Relay States (FC01)

```
FC=0x01  StartAddr=0x0000  Qty=0x000B
Response byte: bits 0–10 correspond to coils 00001–00011
```

---

### 10.9 Write Relay 1 ON (FC05)

```
FC=0x05  CoilAddr=0x0005  Value=0xFF00
```

---

## 11. Modbus Poll / ModScan Configuration

### Modbus Poll Setup

1. **Connection → Connect** → Select `Modbus TCP/IP`
2. IP address: `<board-ip>` (check OLED or web portal)
3. Port: `502`
4. Mode: `TCP`
5. **Setup → Read/Write Definition:**
   - Slave ID: `1`
   - Function: `03 Read Holding Registers`
   - Address: `0` (PDU 0x0000 = Modbus Poll display 40001)
   - Quantity: `25`
   - Scan rate: `500 ms`

### Data Type Display in Modbus Poll

For FLOAT32 pairs, right-click a register → **Display** → **Float** (combines 2 regs, Hi first).

For UINT32 pairs, right-click → **Display** → **32-bit unsigned** (Hi first).

### ModScan32 Setup

1. **Connection → Connect** → Modbus TCP
2. Device ID: `1`, IP: `<board-ip>`, Port: `502`
3. Starting Address: `40001` (Modbus Register notation)
4. Length: `25`
5. Function: `03`

---

## Appendix A — IEEE 754 Float Encoding Reference

| Float Value | Hex (32-bit) | Hi Word | Lo Word |
|-------------|-------------|---------|---------|
| 0.0 | 0x00000000 | 0x0000 | 0x0000 |
| 1.0 | 0x3F800000 | 0x3F80 | 0x0000 |
| 10.0 | 0x41200000 | 0x4120 | 0x0000 |
| 12.5 | 0x41480000 | 0x4148 | 0x0000 |
| 15.0 | 0x41700000 | 0x4170 | 0x0000 |
| 50.0 | 0x42480000 | 0x4248 | 0x0000 |
| 100.0 | 0x42C80000 | 0x42C8 | 0x0000 |
| 250.0 | 0x43780000 | 0x4378 | 0x0000 |
| 500.0 | 0x43FA0000 | 0x43FA | 0x0000 |
| 3125.0 | 0x4543D000 | 0x4543 | 0xD000 |

Use any IEEE 754 converter (e.g., `h-schmidt.de/FloatConverter`) for other values.

---

## Appendix B — Register Quick Reference Card

```
╔══════════════════════════════════════════════════════════════╗
║  LPG Controller — Modbus TCP  IP:?  Port:502  UnitID:1       ║
╠══════════════════════╦═══════╦══════╦═══════╦════════════════╣
║ Name                 ║ Addr  ║ Type ║  R/W  ║ Notes          ║
╠══════════════════════╬═══════╬══════╬═══════╬════════════════╣
║ Live Weight          ║ 40001 ║ F32  ║  R    ║ kg             ║
║ Tare Weight          ║ 40003 ║ F32  ║  R/W  ║ kg             ║
║ Net Weight           ║ 40005 ║ F32  ║  R    ║ kg             ║
║ Target Weight        ║ 40007 ║ F32  ║  R/W  ║ kg             ║
║ Rate/kg              ║ 40009 ║ F32  ║  R/W  ║ PKR/kg         ║
║ Target Amount        ║ 40011 ║ F32  ║  R/W  ║ PKR            ║
║ Current Amount       ║ 40013 ║ F32  ║  R    ║ PKR            ║
║ Fill State           ║ 40015 ║ U16  ║  R    ║ 0=Idle 6=Done  ║
║ E-Stop OK            ║ 40016 ║ U16  ║  R    ║ 1=OK           ║
║ Cylinder Present     ║ 40017 ║ U16  ║  R    ║ 1=present      ║
║ Nozzle Engaged       ║ 40018 ║ U16  ║  R    ║ 1=engaged      ║
║ Weight Stable        ║ 40019 ║ U16  ║  R    ║ 1=stable       ║
║ Transaction Count    ║ 40020 ║ U32  ║  R    ║                ║
║ Uptime               ║ 40022 ║ U32  ║  R    ║ seconds        ║
║ Command              ║ 40024 ║ U16  ║  W    ║ 1/2/3/4        ║
║ Device ID            ║ 40025 ║ U16  ║  R    ║ 0xA601         ║
╠══════════════════════╬═══════╬══════╬═══════╬════════════════╣
║ Coils                ║ 00001 ║      ║ R     ║ E-Stop…FillAct ║
║ Relays 1–6           ║ 00006 ║      ║ R/W   ║                ║
║ Inputs 1–6           ║ 10001 ║      ║ R     ║ DI             ║
╚══════════════════════╩═══════╩══════╩═══════╩════════════════╝
```
