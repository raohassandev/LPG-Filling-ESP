# LPG Controller Modbus TCP/RTU Protocol Reference

Current date: 2026-05-13

Controller Modbus registers are the single source of truth for process and diagnostic values. The display dashboard, controller webpage, Modbus Poll, future HMI/SCADA, and this manual must match `firmware/lpg_controller/include/ModbusRegisterMap.h` and `firmware/lpg_controller/src/ModbusRegisterMap.cpp`.

Addresses are 0-based PDU addresses. Some Modbus tools display Holding Register addresses as 40001 + PDU address.

Old 0x1001 map is legacy only and must not be used for ESP32-S3 display, Haiwell HMI, Modbus Poll verification, or current SCADA integration.

## Connection

| Parameter | TCP | RTU |
|---|---|---|
| Port/interface | TCP port 502 | KC868-A6 RS485 UART2 |
| Slave/unit | Unit ID 1 recommended | Slave address 1 default |
| Baud | n/a | **115200 default** (factory default for new devices) |
| Format | n/a | 8N1 default |
| RX/TX | n/a | RX GPIO14, TX GPIO27 |
| Function codes | FC01, FC02, FC03, FC04, FC05, FC06, FC16 | FC01, FC02, FC03, FC04, FC05, FC06, FC16 |

Float32 and UINT32 use two consecutive 16-bit registers, high word first. The table below shows only the starting address and number of registers.

## RTU Baud Rate Selection

**Recommended default for new installations: 115200 8N1.**

Supported compatibility baud rates: 9600, 19200, 38400, 57600, 115200.

Devices already configured with a saved baud rate continue to use their saved value. Factory default (no saved NVS setting) is 115200.

Lower baud rates are supported for compatibility with older equipment but result in longer physical wire time for large register reads:

| Baud rate | Wire time: 19-reg FC03 read+response | Wire time: 120-reg full map |
|---|---|---|
| 9600 | ~35 ms | ~220 ms |
| 19200 | ~18 ms | ~110 ms |
| 38400 | ~9 ms | ~55 ms |
| 57600 | ~6 ms | ~37 ms |
| 115200 | ~3 ms | ~19 ms |

**HMI/SCADA recommendation:** Use 115200 for new installations. Do not poll slower than necessary — the firmware is designed to respond as fast as the serial frame allows. Read contiguous register blocks rather than single registers to reduce round-trips.

## Performance Design

- FC03/FC04 read requests are served from a RAM register cache updated every main loop iteration for process/IO values (~1 ms) and every 500–1000 ms for RTC/resource data.
- No HX711, filesystem, MQTT, or web work is performed inside the Modbus read path.
- Application Load Percent is a main-loop utilisation estimate, not a guaranteed FreeRTOS CPU load measurement.
- Total RTU transaction time = firmware processing time (a few hundred µs from RAM) + physical serial frame time (baud dependent, see table above).
- HMI/SCADA should not assume slow polling is required. Poll at the rate needed for the application.

Device ID register `0x0018` returns `0xA601`. If Modbus Poll signed decimal display shows `-23039`, that equals unsigned `0xA601`; it is not an error.

## Holding Registers

| Decimal Address | Hex Address | Description | Data Type | Access | Scale Factor | Number of Registers | Value Example |
|---:|---|---|---|---|---:|---:|---|
| 0000 | 0x0000 | Live Weight | Float32 | R | 1.0 | 2 | -0.053 kg |
| 0002 | 0x0002 | Tare Weight | Float32 | RW | 1.0 | 2 | 0.000 kg |
| 0004 | 0x0004 | Net Weight | Float32 | R | 1.0 | 2 | 0.000 kg |
| 0006 | 0x0006 | Target Weight | Float32 | RW | 1.0 | 2 | 12.000 kg |
| 0008 | 0x0008 | Rate Per Kg | Float32 | RW | 1.0 | 2 | 250.000 PKR/kg |
| 0010 | 0x000A | Target Amount | Float32 | RW | 1.0 | 2 | 3000.000 PKR |
| 0012 | 0x000C | Current Amount | Float32 | R | 1.0 | 2 | 0.000 PKR |
| 0014 | 0x000E | Fill State | UINT16 | R | 1 | 1 | 1 |
| 0015 | 0x000F | E-Stop OK | UINT16 | R | 1 | 1 | 1 |
| 0016 | 0x0010 | Cylinder Present | UINT16 | R | 1 | 1 | 1 |
| 0017 | 0x0011 | Nozzle Engaged | UINT16 | R | 1 | 1 | 1 |
| 0018 | 0x0012 | Weight Stable | UINT16 | R | 1 | 1 | 1 |
| 0019 | 0x0013 | Transaction Count | UINT32 | R | 1 | 2 | 6 |
| 0021 | 0x0015 | Uptime Seconds | UINT32 | R | 1 | 2 | 8226 |
| 0023 | 0x0017 | Command | UINT16 | W | 1 | 1 | 1 |
| 0024 | 0x0018 | Device ID | UINT16 | R | 1 | 1 | 0xA601 |
| 0025 | 0x0019 | RTU Slave Address | UINT16 | RW | 1 | 1 | 1 |
| 0026 | 0x001A | RTU Baud Rate | UINT32 | RW | 1 | 2 | 115200 |
| 0028 | 0x001C | RTU Parity | UINT16 | RW | 1 | 1 | 0 |
| 0029 | 0x001D | RTU Stop Bits | UINT16 | RW | 1 | 1 | 1 |
| 0030 | 0x001E | TCP Port | UINT16 | R | 1 | 1 | 502 |
| 0031 | 0x001F | MQTT Connected | UINT16 | R | 1 | 1 | 0 |
| 0032 | 0x0020 | RTC Year | UINT16 | RW | 1 | 1 | 2026 |
| 0033 | 0x0021 | RTC Month | UINT16 | RW | 1 | 1 | 5 |
| 0034 | 0x0022 | RTC Day | UINT16 | RW | 1 | 1 | 8 |
| 0035 | 0x0023 | RTC Hour | UINT16 | RW | 1 | 1 | 20 |
| 0036 | 0x0024 | RTC Minute | UINT16 | RW | 1 | 1 | 30 |
| 0037 | 0x0025 | RTC Second | UINT16 | RW | 1 | 1 | 0 |
| 0038 | 0x0026 | RTC Unix Timestamp | UINT32 | R | 1 | 2 | 1778262600 |
| 0040 | 0x0028 | All-Time Completed Fills | UINT32 | R | 1 | 2 | 0 |
| 0042 | 0x002A | All-Time Failed Fills | UINT32 | R | 1 | 2 | 0 |
| 0044 | 0x002C | All-Time Kg Sold | Float32 | R | 1.0 | 2 | 0.000 kg |
| 0046 | 0x002E | All-Time Amount | Float32 | R | 1.0 | 2 | 0.000 PKR |
| 0048 | 0x0030 | Today Completed Fills | UINT16 | R | 1 | 1 | 0 |
| 0049 | 0x0031 | Today Failed Fills | UINT16 | R | 1 | 1 | 0 |
| 0050 | 0x0032 | Today Kg Sold | Float32 | R | 1.0 | 2 | 0.000 kg |
| 0052 | 0x0034 | Today Amount | Float32 | R | 1.0 | 2 | 0.000 PKR |
| 0054 | 0x0036 | Week Completed Fills | UINT16 | R | 1 | 1 | 0 |
| 0055 | 0x0037 | Week Failed Fills | UINT16 | R | 1 | 1 | 0 |
| 0056 | 0x0038 | Week Kg Sold | Float32 | R | 1.0 | 2 | 0.000 kg |
| 0058 | 0x003A | Week Amount | Float32 | R | 1.0 | 2 | 0.000 PKR |
| 0060 | 0x003C | Month Completed Fills | UINT16 | R | 1 | 1 | 0 |
| 0061 | 0x003D | Month Failed Fills | UINT16 | R | 1 | 1 | 0 |
| 0062 | 0x003E | Month Kg Sold | Float32 | R | 1.0 | 2 | 0.000 kg |
| 0064 | 0x0040 | Month Amount | Float32 | R | 1.0 | 2 | 0.000 PKR |
| 0066 | 0x0042 | Year Completed Fills | UINT16 | R | 1 | 1 | 0 |
| 0067 | 0x0043 | Year Failed Fills | UINT16 | R | 1 | 1 | 0 |
| 0068 | 0x0044 | Year Kg Sold | Float32 | R | 1.0 | 2 | 0.000 kg |
| 0070 | 0x0046 | Year Amount | Float32 | R | 1.0 | 2 | 0.000 PKR |
| 0072 | 0x0048 | Alarm Code | UINT16 | R | 1 | 1 | 0 |
| 0073 | 0x0049 | Alarm Severity | UINT16 | R | 1 | 1 | 0 |
| 0074 | 0x004A | Readiness Mask | UINT16 | R | 1 | 1 | 0x003F |
| 0075 | 0x004B | Blocker Mask | UINT16 | R | 1 | 1 | 0 |
| 0076 | 0x004C | Scale Initialized | UINT16 | R | 1 | 1 | 1 |
| 0077 | 0x004D | Scale Read Error | UINT16 | R | 1 | 1 | 0 |
| 0078 | 0x004E | Calibration Valid | UINT16 | R | 1 | 1 | 1 |
| 0079 | 0x004F | Simulation Active | UINT16 | R | 1 | 1 | 0 |
| 0080 | 0x0050 | Alarm Source | UINT16 | R | 1 | 1 | 0 |
| 0081 | 0x0051 | Application Load Percent | Float32 | R | 1.0 | 2 | 12.5 % |
| 0083 | 0x0053 | Main Loop Average Time | Float32 | R | 1.0 | 2 | 4.2 ms |
| 0085 | 0x0055 | Main Loop Maximum Time | Float32 | R | 1.0 | 2 | 18.7 ms |
| 0087 | 0x0057 | Heap Total Memory | UINT32 | R | 1 | 2 | 327680 bytes |
| 0089 | 0x0059 | Heap Free Memory | UINT32 | R | 1 | 2 | 185000 bytes |
| 0091 | 0x005B | Heap Minimum Free Memory | UINT32 | R | 1 | 2 | 160000 bytes |
| 0093 | 0x005D | Heap Free Percent | Float32 | R | 1.0 | 2 | 56.4 % |
| 0095 | 0x005F | PSRAM Total Memory | UINT32 | R | 1 | 2 | 0 bytes |
| 0097 | 0x0061 | PSRAM Free Memory | UINT32 | R | 1 | 2 | 0 bytes |
| 0099 | 0x0063 | PSRAM Free Percent | Float32 | R | 1.0 | 2 | 0.0 % |
| 0101 | 0x0065 | Flash Size | UINT32 | R | 1 | 2 | 4194304 bytes |
| 0103 | 0x0067 | Firmware Sketch Size | UINT32 | R | 1 | 2 | 890000 bytes |
| 0105 | 0x0069 | Free Sketch Space | UINT32 | R | 1 | 2 | 1200000 bytes |
| 0107 | 0x006B | ESP32 Internal Chip Temperature | Float32 | R | 1.0 | 2 | 42.5 C |
| 0109 | 0x006D | WiFi RSSI | INT16 | R | 1 | 1 | -55 dBm |
| 0110 | 0x006E | WiFi Status | UINT16 | R | 1 | 1 | 3 |
| 0111 | 0x006F | MQTT Client State | INT16 | R | 1 | 1 | 0 |
| 0112 | 0x0070 | Last Reset Reason | UINT16 | R | 1 | 1 | 1 |
| 0113 | 0x0071 | Firmware Build Mode | UINT16 | R | 1 | 1 | 1 |
| 0114 | 0x0072 | Modbus RTU Request Count | UINT32 | R | 1 | 2 | 1250 |
| 0116 | 0x0074 | Modbus RTU Error Count | UINT32 | R | 1 | 2 | 3 |
| 0118 | 0x0076 | Controller Heartbeat Counter | UINT32 | R | 1 | 2 | 22050 |

Addresses 0x0078–0x007F are reserved.

## HMI Operation Block (0x0080–0x009B)

Holding registers 0x0080–0x009B implement a structured command/response interface for Weintek HMI panels and similar operator terminals. The HMI writes commands and preset values; the controller executes them in the main loop and writes back status flags.

Float32 registers in this block use the same big-endian high-word-first layout as the rest of the map.

| Decimal | Hex | Description | Type | Access | Notes |
|---:|---|---|---|---|---|
| 0128 | 0x0080 | HMI Command Code | UINT16 | RW | kHmiCmd_* — write before updating Seq |
| 0129 | 0x0081 | HMI Command Sequence | UINT16 | RW | Increment to trigger; echo in LastAcceptedSeq confirms execution |
| 0130 | 0x0082 | HMI Last Accepted Seq | UINT16 | R | Mirrors CommandSeq after command is consumed |
| 0131 | 0x0083 | HMI Command Result | UINT16 | R | kHmiResult_* — read after seq echo |
| 0132 | 0x0084 | HMI Command Error Code | UINT16 | R | kHmiErr_* — non-zero on Rejected/Failed |
| 0133 | 0x0085 | HMI Command Busy | UINT16 | R | 1 while controller is processing |
| 0134 | 0x0086 | HMI Fill Mode | UINT16 | RW | 0 = by-kg, 1 = by-amount |
| 0135 | 0x0087 | HMI Prepared Flag | UINT16 | R | 1 = preset validated, PrepareNext succeeded |
| 0136 | 0x0088 | HMI Ready To Prepare | UINT16 | R | 1 = safety + scale OK, may call PrepareNext |
| 0137 | 0x0089 | HMI Ready To Start | UINT16 | R | 1 = prepared + safe + weight stable |
| 0138 | 0x008A | HMI Can Tare | UINT16 | R | 1 = tare safe (no fill active, no fault) |
| 0139 | 0x008B | HMI Can Stop | UINT16 | R | 1 = fill is active, Stop command valid |
| 0140 | 0x008C | HMI Heartbeat | UINT16 | RW | HMI writes any value to reset watchdog age |
| 0141 | 0x008D | HMI WDT Timeout Sec | UINT16 | RW | 0 = watchdog disabled; default 30 s |
| 0142 | 0x008E | HMI Heartbeat Age Sec | UINT16 | R | Seconds since last heartbeat write |
| 0143 | 0x008F | HMI Reserved | UINT16 | R | Always 0 |
| 0144 | 0x0090 | HMI Preset Tare | Float32 | RW | Tare weight in kg (high word first) |
| 0146 | 0x0092 | HMI Preset Target Weight | Float32 | RW | Target fill weight in kg (by-kg mode) |
| 0148 | 0x0094 | HMI Preset Rate Per Kg | Float32 | RW | Price per kg in PKR/kg (by-amount mode) |
| 0150 | 0x0096 | HMI Preset Target Amount | Float32 | RW | Target amount in PKR (by-amount mode) |
| 0152 | 0x0098 | HMI Preset Valid | UINT16 | R | 1 = last PrepareNext validated preset OK |
| 0153 | 0x0099 | HMI Preset Error Code | UINT16 | R | kHmiErr_* from last PrepareNext |
| 0154 | 0x009A | HMI Last Fill Result | UINT16 | R | kHmiResult_* of the most recent fill |
| 0155 | 0x009B | HMI Last Fill Error Code | UINT16 | R | kHmiErr_* of the most recent fill |

### HMI Command Codes

| Value | Command | Description |
|---:|---|---|
| 0 | None | No-op |
| 10 | PrepareNext | Validate preset values and set PreparedFlag. Requires: e-stop OK, no fault, scale ready, calibration valid. |
| 11 | ApplyTare | Apply PresetTare to the tare register immediately (no fill required). |
| 12 | ZeroNet | Set tare = current live weight (zero the net). |
| 13 | RequestTare | Trigger non-blocking HX711 hardware tare. Only valid when Idle or Ready. |
| 20 | ModeKg | Set fill mode to by-kg (uses Preset Target Weight). |
| 21 | ModeAmount | Set fill mode to by-amount (uses Preset Rate + Target Amount). |
| 30 | Start | Start fill using validated preset. Requires PreparedFlag=1, e-stop OK, weight stable. |
| 31 | Stop | Stop an active fill. |
| 32 | Reset | Reset controller to Idle. Clears PreparedFlag. |
| 33 | AckComplete | Acknowledge fill complete and clear PreparedFlag. |
| 40 | ClearResult | Reset CommandResult and CommandErrCode to idle/none. |

### HMI Result Codes

| Value | Result |
|---:|---|
| 0 | Idle |
| 1 | Accepted — command queued |
| 2 | Busy — processing |
| 3 | Rejected — see ErrorCode |
| 4 | Done |
| 5 | Failed — see ErrorCode |

### HMI Error Codes

| Value | Error |
|---:|---|
| 0 | None |
| 1 | Invalid command code |
| 2 | Invalid sequence |
| 3 | Controller busy |
| 4 | Not prepared — call PrepareNext first |
| 5 | Safety not ready (e-stop, cylinder, nozzle) |
| 6 | Scale not ready (not initialized or read error) |
| 7 | Weight not stable |
| 8 | Calibration invalid |
| 9 | Invalid preset values |
| 10 | Command not allowed in current state |
| 11 | Watchdog timeout — heartbeat expired |
| 12 | Fill fault active |

### HMI Command Workflow

**By-kg fill:**
1. Write Preset Tare (0x0090–0x0091) as Float32.
2. Write Preset Target Weight (0x0092–0x0093) as Float32.
3. Write FillMode=0 to 0x0086.
4. Write CommandCode=10 to 0x0080, then increment CommandSeq at 0x0081.
5. Wait for LastAcceptedSeq (0x0082) to echo the sequence value.
6. Check CommandResult (0x0083): Done=4 means prepared, Rejected=3 means see ErrorCode.
7. Poll ReadyToStart (0x0089); once 1, write CommandCode=30 + new seq.
8. Confirm Fill State transitions (Idle→Ready→Validating→Fast→Slow→Settling→Complete).
9. When Complete, write CommandCode=33 (AckComplete) + new seq.

**Watchdog:** Write any value to HMI Heartbeat (0x008C) at least every WdtTimeout seconds to prevent watchdog rejection of Start/PrepareNext commands. Disable by writing 0 to WdtTimeout (0x008D).

## Resource Register Notes

`Application Load Percent` is a main-loop load estimate based on observed loop execution time against the nominal 20 ms loop period. It is not true CPU load.

`ESP32 Internal Chip Temperature` is the internal ESP32 die temperature. It is not ambient temperature. If a future SDK/target cannot read chip temperature, firmware must return an unsupported value and UI should show N/A, not a fake number.

`WiFi Status` uses the ESP32 `WiFi.status()` value. In Arduino ESP32, `3` commonly means connected.

`MQTT Client State` is currently `0 = disconnected`, `1 = connected`.

`Firmware Build Mode` values: `0 = Production`, `1 = Prototype`, `2 = Development`.

Modbus RTU request/error counters are real counters from the RTU service. Request count increments on valid handled RTU requests. Error count increments on CRC error or Modbus exception paths.

## Command Values

| Value | Command |
|---:|---|
| 1 | Start Fill |
| 2 | Stop Fill |
| 3 | Reset to Idle |
| 4 | Zero Net / tare current live weight |

Production builds reject Modbus writes unless `LPG_MODBUS_WRITES_ENABLED=1` or `LPG_PROTOTYPE_BUILD=1` is explicitly used. Prototype builds can write target/rate/amount and command registers.

## Fill State Values

| Value | State |
|---:|---|
| 0 | Idle |
| 1 | Ready |
| 2 | Validating |
| 3 | Filling Fast |
| 4 | Filling Slow |
| 5 | Settling |
| 6 | Complete |
| 7 | Aborted |
| 8 | Fault |
| 9 | Maintenance |

## Alarm Code Values

| Value | Alarm |
|---:|---|
| 0 | None |
| 1 | Emergency stop active |
| 2 | Nozzle disengaged |
| 3 | Cylinder missing |
| 4 | Scale read error |
| 5 | Scale not stable |
| 6 | Scale not calibrated |
| 7 | Overfill |
| 8 | Fill timeout |
| 9 | No flow |
| 10 | Transaction log fault |
| 11 | Operator stop |
| 12 | Active controller fault |
| 13 | Controller offline, display-local alarm only |

Alarm severity values: `0 = none`, `1 = info`, `2 = warning`, `3 = alarm/fault`.

Alarm source values: `0 = none`, `1 = safety`, `2 = scale`, `3 = process`, `4 = operator/comms`.

## Readiness And Blocker Masks

Readiness mask bits:

| Bit | Meaning |
|---:|---|
| 0 | E-stop OK |
| 1 | Cylinder present |
| 2 | Nozzle engaged |
| 3 | Weight stable |
| 4 | Calibration valid |
| 5 | Scale initialized and no scale read error |

Blocker mask bits:

| Bit | Meaning |
|---:|---|
| 0 | Controller fault state |
| 1 | E-stop not OK |
| 2 | Cylinder missing |
| 3 | Nozzle not engaged |
| 4 | Scale not initialized |
| 5 | Scale read error |
| 6 | Weight not stable |
| 7 | Calibration invalid |
| 8 | Simulation active |

## Coils

| Decimal Address | Hex Address | Description | Access |
|---:|---|---|---|
| 0000 | 0x0000 | E-Stop OK | R |
| 0001 | 0x0001 | Cylinder Present | R |
| 0002 | 0x0002 | Nozzle Engaged | R |
| 0003 | 0x0003 | Weight Stable | R |
| 0004 | 0x0004 | Fill Active | R |
| 0005 | 0x0005 | Relay 1 | RW |
| 0006 | 0x0006 | Relay 2 | RW |
| 0007 | 0x0007 | Relay 3 | RW |
| 0008 | 0x0008 | Relay 4 | RW |
| 0009 | 0x0009 | Relay 5 | RW |
| 0010 | 0x000A | Relay 6 | RW |

FC05 coil write values: `0xFF00 = on`, `0x0000 = off`.

## Discrete Inputs

| Decimal Address | Hex Address | Description | Access |
|---:|---|---|---|
| 0000 | 0x0000 | Input 1 | R |
| 0001 | 0x0001 | Input 2 | R |
| 0002 | 0x0002 | Input 3 | R |
| 0003 | 0x0003 | Input 4 | R |
| 0004 | 0x0004 | Input 5 | R |
| 0005 | 0x0005 | Input 6 | R |

## Start Fill Sequence (legacy direct-write path)

This sequence uses the raw Modbus write registers. For HMI panels, use the HMI Operation Block at 0x0080–0x009B instead.

1. Write Target Weight to `0x0006` as Float32 with FC16.
2. Write Rate Per Kg to `0x0008` as Float32 with FC16.
3. Write Target Amount to `0x000A` as Float32 with FC16.
4. Read back `0x0006`, `0x0008`, and `0x000A`.
5. Only after readback matches, write command `1` to `0x0017`.
6. Confirm Fill State changes from Idle/Ready into Validating/Fast/Slow/Settling/Complete.

## Manufacturing PIN

Certain web portal actions require the Manufacturing PIN:

- Load cell calibration (hardware tare, set calibration point, save/revert)
- OTA firmware upload

The Manufacturing PIN is a 6-digit number generated once on first boot and saved to NVS. It is printed to the serial console at boot-time and is labelled on the unit. It is not transmitted over Modbus and is never stored in query strings.

Rate limiting: 5 failed attempts per 60-second window locks out further attempts. The device must remain powered for the window to expire.

The PIN is passed in the `X-MFG-PIN` HTTP request header. It is stored in browser localStorage as `lpgMfgPin` by the calibration and OTA web pages.

## OTA Firmware Update

OTA firmware is uploaded via the web portal at `/ota.html` or via a direct `POST /api/ota/upload` with the binary `.bin` file and the `X-MFG-PIN` header. OTA is blocked while a fill is active. The device restarts automatically after a successful upload and boots the new firmware. The previous firmware slot is retained for rollback if the next boot fails.
