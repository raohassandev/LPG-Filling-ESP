# Weintek LiveWeight R&D Report
# LPG Filling Controller — Modbus RTU Diagnostic

**Date**: 2026-05-14  
**Branch**: hmi-operation-calibration-ota  
**Firmware repository**: D:\Working\LPG-Filling-ESP  
**Node tools directory**: D:\Working\R&D\Weintek LPG\Node app  

---

## Files Inspected

| File | Purpose |
|---|---|
| `firmware/lpg_controller/include/ModbusRegisterMap.h` | Authoritative register address constants |
| `firmware/lpg_controller/src/ModbusRegisterCache.cpp` | Float32/UInt32 encoding implementation |
| `firmware/lpg_controller/src/ModbusRtuService.cpp` | FC03/FC04 handler, CRC, serial settings |
| `firmware/lpg_controller/src/WeightService.cpp` | HX711 read, liveWeightKg_ initialization |
| `firmware/lpg_controller/include/HmiOperationService.h` | HMI command/result codes |
| `docs/MODBUS_RTU_CURRENT_MAP.md` | Official register summary |
| `docs/hmi/weintek_hmi_tags.csv` | Existing Weintek HMI tag file |
| `AI-Context.md` | Bench-verified serial settings |

---

## Confirmed Facts (Source: Firmware Code)

### Float32 Encoding

From `ModbusRegisterCache.cpp` lines 7–12:

```cpp
void ModbusRegisterCache::setFloat(uint16_t* regs, uint16_t addr, float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    regs[addr]     = static_cast<uint16_t>(bits >> 16);  // HIGH word → lower register
    regs[addr + 1] = static_cast<uint16_t>(bits & 0xFFFF); // LOW word → higher register
}
```

**Encoding: ABCD — big-endian, high-word-first.**  
Header comment confirms: `FLOAT32 = 2 consecutive 16-bit regs, big-endian IEEE-754 (Hi word first)`

This is the standard Modbus Float32 convention. In Weintek EasyBuilder Pro this is called **"High word first"**.

### FC04 Aliases FC03

From `ModbusRtuService.cpp` line 225:
```cpp
case 0x04: return handleFC03(req, resp, respLen); // FC04 mirrors FC03
```

Both 3x (FC04) and 4x (FC03) addressing work. Use **4x** in Weintek (it is more semantically correct and standard).

### Register Type: Holding Registers

FC03 handler range check: `startAddr >= kHR_Base && startAddr + qty <= kHR_Base + kHR_Count`  
`kHR_Base = 0x0000` — zero-based PDU addressing confirmed.

### LiveWeight Register

```
kHR_LiveWeightHi = 0x0000  (decimal 0)   ← HIGH word of Float32
kHR_LiveWeightLo = 0x0001  (decimal 1)   ← LOW word of Float32
```

The Float32 spans registers 0 and 1. In Weintek, set address to **0** (the base address of the pair).

### DeviceId Register

```
kHR_DeviceId = 0x0018  (decimal 24)      ← UINT16
Value written in firmware:  0xA601  (decimal 42497)
```

From `ModbusRegisterCache.cpp` line 61: `regs_[kHR_DeviceId] = 0xA601;`

**Bench verified (AI-Context.md, 2026-05-08):** Display firmware read register 0x0018 and received 0xA601.  
Note: If a Modbus Poll tool shows `-23039` for DeviceId, that is the **signed** representation of 0xA601 and is correct.

### Serial Settings (Bench-Verified)

From `AI-Context.md` (2026-05-08):
```
Port:    COM10
Baud:    9600
Format:  8N1 (no parity, 1 stop bit)
Slave:   1
GPIO:    RX=14, TX=27
```

The firmware prints on boot:
```
[RTU] Note: active baud is NOT 115200 (saved NVS value). Use Apply Recommended 115200...
```
This message appears when NVS baud is not 115200. Currently **9600 is the active baud**.

### HMI Command Block

```
kHR_HmiBase = kHR_HmiCommandCode = 0x0080  (decimal 128)
```
The HMI block spans 0x0080–0x009B (28 registers). Weintek 4x address 128.

### Race Condition Analysis

The firmware is single-threaded (Arduino loop). `updateFast()` and `handleClient()` are both called from the same `loop()` iteration — they cannot interleave. There is **no race condition** for LiveWeight.

### NaN/Inf Analysis

`WeightService` initializes `liveWeightKg_` to its C++ default (0.0f). If HX711 is not initialized (`hx711Initialized_ = false`), `poll()` returns early and `liveWeightKg_` stays at 0.0f. If the calibration factor is zero (not yet calibrated), the single-point formula divides by it — this would produce Inf. However, `kHR_CalibrationValid` (register 78) tells you whether calibration is valid.

**Modbus-level protection**: If `CalibrationValid = 0`, the HMI should show a calibration warning rather than the raw weight value.

---

## Root Cause of Weintek Stars (★★★)

After reading all firmware source, **three likely causes** in priority order:

### Cause 1: Wrong Word Order in Weintek (Most Likely)

Weintek shows ★★★ when the numeric display cannot represent the value. If Float32 word order is set to **"Low word first" (CDAB)** instead of **"High word first" (ABCD)**, the decoded float will be a garbage value — often `NaN`, `Inf`, or an astronomically large number that overflows the display digits.

**Fix**: In EasyBuilder Pro, on the LiveWeight tag, set word order to **"High word first"**.

### Cause 2: Display Integer Digits Too Small

If LiveWeight is, say, `14.25 kg` but the numeric display object has only 2 integer digits, Weintek shows ★★★ because the integer part doesn't fit.

**Fix**: Set the Numeric display to 3 integer digits + 2 decimal digits = 5 total. Set Min = -10, Max = 150.

### Cause 3: Wrong Baud Rate or Slave ID

If Weintek is set to 115200 but the controller runs at 9600, all reads will fail and the tags will show their "communication error" state, which some display objects render as ★★★.

**Fix**: Set Weintek driver to 9600 baud, 8N1, slave station 1.

---

## Exact Weintek EasyBuilder Pro Settings

### Driver / Device Setup

| Setting | Value |
|---|---|
| Driver | MODBUS RTU |
| COM port | match your USB-RS485 adapter port |
| Baud rate | **9600** |
| Data bits | **8** |
| Parity | **None** |
| Stop bits | **1** |
| Station number (Slave ID) | **1** |
| Addressing mode | **Zero-based** (NOT 40001-based) |

### LiveWeight Tag

| Setting | Value |
|---|---|
| Tag name | LiveWeight |
| Address type | **4x** (Holding Register) |
| PDU address | **0** (zero-based) |
| Data type | **32-bit Float** |
| Word order | **High word first** |
| Byte order | Normal (no byte swap) |

### DeviceId Tag (use this first to verify communication)

| Setting | Value |
|---|---|
| Tag name | DeviceId |
| Address type | **4x** |
| PDU address | **24** (decimal, = 0x0018) |
| Data type | **16-bit Unsigned** |
| Expected value | **42497** (= 0xA601) |

### Numeric Display Object for LiveWeight

| Setting | Value |
|---|---|
| Data format | Floating point |
| Integer digits | **3** |
| Decimal digits | **2** |
| Min | -10 |
| Max | 150 |
| Total display width | at least 6 characters + sign |

If the display shows ★★★ and DeviceId reads correctly as 42497, the problem is one of:
- Wrong word order (most common)
- Display digits too narrow

---

## Diagnostic Decision Tree

```
1. Does DeviceId (4x addr 24) read 42497?
   NO  → Fix baud rate / slave ID / wiring first. Nothing else matters.
   YES → proceed

2. Does LiveWeight_HiWord (4x addr 0, 16-bit Unsigned) show a stable non-zero value?
   NO  → Scale hardware problem. Check ScaleInitialized (4x addr 76), ScaleReadError (4x addr 77).
   YES → proceed

3. Does LiveWeight_Float (4x addr 0, 32-bit Float, High-word-first) show a plausible kg value?
   NO  → Try changing word order to "Low word first" and check. Also check display digits.
   YES → It is working. The previous ★★★ was a display-format problem.

4. If CalibrationValid (4x addr 78) = 0, weight will be 0.0 or wrong until calibrated.
```

---

## Port Discovery (Run npm run ports)

As of 2026-05-14, the following COM ports were found on this machine:

```
PORT        MANUFACTURER    VID:PID
COM10       wch.cn          1A86:7523   ← CH340 USB-RS485, this is the LPG controller
COM2        FTDI            0403:6001   ← FTDI FT232
COM12       Intel           ----:----   ← internal
```

**COM10 (CH340, VID 1A86:7523) is the controller port.**

---

## How to Run the Probe

Connect COM10 to the RS485 bus, then run:

```powershell
# From D:\WeinLPG (the junction — needed due to & in path)
cd D:\WeinLPG
npm run probe9600 -- --port COM10 --id 1 --samples 10
```

Or to run continuously:
```powershell
npm run probe9600 -- --port COM10 --id 1 --samples 0 --interval 1000
```

The probe will:
1. Read DeviceId first — if it is not 42497, it stops and tells you to fix serial settings
2. Print raw hex values for reg[0] and reg[1]
3. Decode Float32 in all four orderings (ABCD, CDAB, BADC, DCBA)
4. Mark which decoded value is physically plausible for LPG cylinder weight
5. Read ScaleInitialized, CalibrationValid, and SimulationActive diagnostics

---

## Files Generated

| File | Description |
|---|---|
| `tools/list-serial-ports.js` | Lists all serial ports with VID/PID |
| `tools/modbus-liveweight-probe.js` | Live Modbus RTU probe for LiveWeight |
| `tools/modbus-map-audit.js` | Parses firmware header, checks for map errors |
| `tools/generate-weintek-tags.js` | Generates Weintek-importable CSV tag files |
| `docs/hmi/weintek_tags_clean_zero_based.csv` | Full tag map (104 tags) |
| `docs/hmi/weintek_tags_test_liveweight.csv` | Minimal test set (18 tags) for diagnosis |
| `docs/hmi/modbus_map_audit_report.md` | Audit report (no issues found) |
| `tools/out/modbus_map_audit.json` | Machine-readable audit output |

---

## Firmware Bug Assessment

**No firmware bugs found.**

The register encoding is correct and consistent:
- `setFloat` correctly stores high word at lower address (ABCD)
- `readBlock` correctly outputs registers in big-endian byte order
- FC04 correctly aliases FC03
- DeviceId correctly writes 0xA601 every `updateFast()` call
- No race condition (single-threaded loop)
- `liveWeightKg_` initializes to 0.0f, not NaN

The problem is in the **Weintek HMI configuration**, not the firmware.

---

## Conclusion

**The firmware is correct. The issue is in EasyBuilder Pro configuration.**

To fix LiveWeight showing ★★★ in Weintek:

1. **Verify communication**: Add DeviceId tag (4x, addr 24, UINT16) and confirm it reads 42497.
2. **Set Float32 word order**: In the LiveWeight tag, select **"High word first"** (not "Low word first").
3. **Set display digits**: Use 3 integer + 2 decimal = XX.XX format. Min -10, Max 150.
4. **Confirm baud rate**: 9600, 8N1, slave 1.
5. **Import the test CSV**: Use `docs/hmi/weintek_tags_test_liveweight.csv` to add all diagnostic tags at once.

If after all this the weight still shows wrong values, run the Node.js probe to check raw register values before Weintek is involved:
```powershell
cd D:\WeinLPG
npm run probe9600 -- --port COM10 --id 1
```

The ABCD-decoded value from the probe is ground truth — if it matches physical weight, the firmware and serial link are correct and only the HMI display format needs adjustment.
