# LPG Controller Modbus and Real-Time Protocol

This document is the third-party integration contract for HMI, SCADA, gateway, and mobile app teams.

## Current Working Firmware Snapshot

This project now has two Modbus surfaces:

- Display-to-controller production path: Modbus RTU over RS485.
- Optional integration path: Modbus TCP on controller port `502`.

The display firmware uses the RTU map in `firmware/lpg_controller/include/ModbusRegisterMap.h`, with 0-based holding register addresses. Do not use the older `0x1001` prototype map for the display firmware.

Old 0x1001 map is legacy only and must not be used for ESP32-S3 display, Haiwell HMI, Modbus Poll verification, or current SCADA integration.

Known-good RTU settings:

| Item | Display | Controller |
| --- | --- | --- |
| UART | `UART_NUM_1` | `Serial2` |
| TX | GPIO44 | GPIO27 |
| RX | GPIO43 | GPIO14 |
| Baud | 9600 | 9600 |
| Data/parity/stop | 8N1 | 8N1 |
| Slave/unit id | 1 | 1 |
| Direction control | auto | auto, `kRtuDePin = 255` |

Known-good controller serial console:

- Port: CH340, usually `COM4`
- Baud: `115200`
- Commands: `status`, `hx`, `rtu`, `start 12 250`, `stop`, `reset`
- Healthy RTU print: `enabled=1 slave=1 baud=9600 data=8 parity=0 stop=1 rx=14 tx=27 de=255`

Delivery debugging rule: if the display says "Controller offline Check RS485" during START but live weight still updates, first test `start 12 250` on controller serial. A controller-side rejection can look like an RS485 failure at the display layer.

## Device Network

- Default STA IP during testing: `192.168.0.108`
- HTTP API port: `80`
- Modbus TCP port: `502`
- Modbus unit id: accepted and echoed, currently not filtered
- Byte order: big-endian Modbus standard

## Legacy Modbus TCP Prototype Map

Legacy reference only. The `0x1001` map below is the older Modbus TCP/prototype integration map. It is retained only for third-party clients that already use it. The display firmware does not use this map, and new HMI/SCADA clients should use the active `0x0000..0x0077` map below.

Supported functions in the older TCP map:

- `0x03`: read holding registers
- `0x06`: write single holding register

Unsupported functions return Modbus exception `0x01`.
Unsupported addresses return exception `0x02`.
Read/write value errors return exception `0x03`.

### Legacy Register Map

All weight values are unsigned 16-bit integers scaled as `kg x 100`.

| Address | Name | Access | Scale | Description |
| --- | --- | --- | --- | --- |
| `0x1001` | Live Weight | Read | kg x 100 | Current gross/live scale weight |
| `0x1002` | Tare Weight | Read/Write | kg x 100 | Empty cylinder tare weight |
| `0x1003` | Net Weight | Read | kg x 100 | `live - tare`, clamped at zero |
| `0x1004` | Filling Status | Read | enum | Firmware `ProcessState` code |
| `0x1005` | Target Weight | Read | kg x 100 | Active fill target net weight |
| `0x1006` | E-stop Status | Read | boolean | `1` = OK, `0` = tripped |

### Status Codes

| Code | State |
| --- | --- |
| `0` | Boot |
| `1` | Idle |
| `2` | Ready |
| `3` | Validating |
| `4` | Filling Fast |
| `5` | Filling Slow |
| `6` | Settling |
| `7` | Complete |
| `8` | Aborted |
| `9` | Fault |
| `10` | Maintenance |

### Example Reads

Read all six controller registers:

- Function: `0x03`
- Start address: `0x1001`
- Quantity: `6`

Read only net weight:

- Function: `0x03`
- Start address: `0x1003`
- Quantity: `1`

If response value for net weight is `1180`, display `11.80 kg`.

### Example Write

Set tare weight to `15.25 kg`:

- Function: `0x06`
- Register: `0x1002`
- Value: `1525`

Only tare weight is writable in the older prototype TCP map.

## Modbus RTU Display Map

This is the active display-to-controller map. All addresses are 0-based PDU holding-register addresses, shown exactly as used by the display firmware.

Supported functions:

- `0x03`: read holding registers
- `0x04`: mirrors holding-register reads
- `0x06`: write single holding register
- `0x10`: write multiple holding registers

Unsupported functions return exception `0x01`.
Unsupported addresses return exception `0x02`.
Invalid values or rejected commands return exception `0x03`.

FLOAT32 values are IEEE-754 high word first.

| Address | Name | Type | Access | Notes |
| --- | --- | --- | --- | --- |
| `0x0000..0x0001` | Live Weight | FLOAT32 | R | kg |
| `0x0002..0x0003` | Tare Weight | FLOAT32 | R/W | kg |
| `0x0004..0x0005` | Net Weight | FLOAT32 | R | kg |
| `0x0006..0x0007` | Target Weight | FLOAT32 | R/W | kg |
| `0x0008..0x0009` | Rate Per Kg | FLOAT32 | R/W | currency/kg |
| `0x000A..0x000B` | Target Amount | FLOAT32 | R/W | currency |
| `0x000C..0x000D` | Current Amount | FLOAT32 | R | `net * rate` |
| `0x000E` | Fill State | UINT16 | R | see status codes |
| `0x000F` | E-stop OK | UINT16 | R | `1` = OK |
| `0x0010` | Cylinder Present | UINT16 | R | `1` = present |
| `0x0011` | Nozzle Engaged | UINT16 | R | `1` = engaged |
| `0x0012` | Weight Stable | UINT16 | R | `1` = stable |
| `0x0017` | Command | UINT16 | W | `1` start, `2` stop, `3` reset, `4` zero-net |
| `0x0019` | RTU Slave Address | UINT16 | R/W | 1-247 |
| `0x001A..0x001B` | RTU Baud | UINT32 | R/W | standard baud only |
| `0x001C` | RTU Parity | UINT16 | R/W | `0` N, `1` E, `2` O |
| `0x001D` | RTU Stop Bits | UINT16 | R/W | `1` or `2` |
| `0x0020..0x0025` | RTC Y/M/D/H/M/S | UINT16 | R/W | writing second commits date/time |
| `0x0030..0x0035` | Today Stats | mixed | R | completed, failed, kg, amount |
| `0x0048` | Alarm Code | UINT16 | R | see alarm table below |
| `0x0049` | Alarm Severity | UINT16 | R | `0` none, `1` info, `2` warning, `3` alarm/fault |
| `0x004A` | Readiness Mask | UINT16 | R | bit0 E-stop OK, bit1 cylinder, bit2 nozzle, bit3 stable, bit4 calibrated, bit5 scale ready |
| `0x004B` | Blocker Mask | UINT16 | R | bit0 fault, bit1 E-stop, bit2 cylinder, bit3 nozzle, bit4 scale init, bit5 read, bit6 stable, bit7 calibration, bit8 simulation |
| `0x004C` | Scale Initialized | UINT16 | R | `1` = HX711 initialized |
| `0x004D` | Scale Read Error | UINT16 | R | `1` = scale read failed |
| `0x004E` | Calibration Valid | UINT16 | R | `1` = calibration valid |
| `0x004F` | Simulation Active | UINT16 | R | `1` = simulated weight active |
| `0x0050` | Alarm Source | UINT16 | R | `0` none, `1` safety, `2` scale, `3` process, `4` operator/comms |
| `0x0051..0x0052` | Application Load Percent | FLOAT32 | R | Main-loop load estimate percent, not true CPU load |
| `0x0053..0x0054` | Main Loop Average Time | FLOAT32 | R | milliseconds |
| `0x0055..0x0056` | Main Loop Maximum Time | FLOAT32 | R | milliseconds |
| `0x0057..0x0058` | Heap Total Memory | UINT32 | R | bytes |
| `0x0059..0x005A` | Heap Free Memory | UINT32 | R | bytes |
| `0x005B..0x005C` | Heap Minimum Free Memory | UINT32 | R | bytes |
| `0x005D..0x005E` | Heap Free Percent | FLOAT32 | R | percent |
| `0x005F..0x0060` | PSRAM Total Memory | UINT32 | R | bytes |
| `0x0061..0x0062` | PSRAM Free Memory | UINT32 | R | bytes |
| `0x0063..0x0064` | PSRAM Free Percent | FLOAT32 | R | percent |
| `0x0065..0x0066` | Flash Size | UINT32 | R | bytes |
| `0x0067..0x0068` | Firmware Sketch Size | UINT32 | R | bytes |
| `0x0069..0x006A` | Free Sketch Space | UINT32 | R | bytes |
| `0x006B..0x006C` | ESP32 Internal Chip Temperature | FLOAT32 | R | die temperature, not ambient |
| `0x006D` | WiFi RSSI | INT16 | R | dBm |
| `0x006E` | WiFi Status | UINT16 | R | ESP32 WiFi.status() value |
| `0x006F` | MQTT Client State | INT16 | R | `0` disconnected, `1` connected |
| `0x0070` | Last Reset Reason | UINT16 | R | esp_reset_reason value |
| `0x0071` | Firmware Build Mode | UINT16 | R | `0` production, `1` prototype, `2` development |
| `0x0072..0x0073` | Modbus RTU Request Count | UINT32 | R | valid handled RTU requests |
| `0x0074..0x0075` | Modbus RTU Error Count | UINT32 | R | CRC/exception error count |
| `0x0076..0x0077` | Controller Heartbeat Counter | UINT32 | R | increments from main loop |

Alarm codes:

| Code | Meaning | Typical cause / display text |
| --- | --- | --- |
| `0` | None | System ready or no active alarm |
| `1` | Emergency stop active | Emergency push button is pressed |
| `2` | Nozzle disengaged | Nozzle lock/engage input is not active |
| `3` | Cylinder missing | Cylinder-present input is not active |
| `4` | Scale read error | HX711/load cell read failed |
| `5` | Scale not stable | Weight is still settling before start |
| `6` | Scale not calibrated | Calibration factor invalid or missing |
| `7` | Overfill | Fill exceeded target/safety limit |
| `8` | Fill timeout | Fill ran beyond allowed time |
| `9` | No flow | Fill active but weight did not increase |
| `10` | Transaction log fault | Controller could not create/update transaction |
| `11` | Operator stop | Fill was stopped by operator, serial, or Modbus |
| `12` | Active controller fault | Controller is in fault state without a more specific code |
| `13` | Controller offline | Display-local alarm when RS485/TCP polling fails |

Display START sequence:

1. Write target weight FLOAT32 to `0x0006..0x0007` using FC16.
2. Write rate FLOAT32 to `0x0008..0x0009` using FC16.
3. Write amount FLOAT32 to `0x000A..0x000B` using FC16.
4. Read back target/rate/amount from `0x0006`, `0x0008`, and `0x000A`.
5. Write command `1` to `0x0017` using FC06 only after readback matches.

The display must hold the Modbus bus mutex across the whole sequence so background polling cannot interleave with START writes.

Controller START prerequisites:

- Not already filling.
- Not in fault.
- E-stop OK.
- Cylinder present.
- Nozzle engaged.
- Target and rate positive.
- HX711 initialized and reading.
- Weight stable.
- Scale calibration valid.
- Transaction log can create a record.

Readiness icons on the display cover visible safety inputs and stability, while the alarm panel now shows controller diagnostics from `0x0048..0x0050`. Controller resource registers are available from `0x0051..0x0077`. If START is rejected, first read `Alarm Code`, `Blocker Mask`, `Scale Initialized`, `Scale Read Error`, `Calibration Valid`, and `Simulation Active`; then use controller serial `hx` and `start 12 250` only if the register values are not enough.

## HTTP API

Base URL example:

```text
http://192.168.0.108
```

Important endpoints:

| Method | Path | Purpose |
| --- | --- | --- |
| `GET` | `/api/status` | Current status, weights, safety inputs, relays |
| `GET` | `/api/settings` | Current setup values, including rate per kg |
| `POST` | `/api/settings?ratePerKg=250` | Save admin rate per kg |
| `POST` | `/api/tare?tareWeightKg=15.25` | Set operator tare weight |
| `POST` | `/api/tare-zero` | Set tare equal to current live weight |
| `POST` | `/api/start?targetWeightKg=11.8&ratePerKg=250&targetAmount=2950` | Start fill |
| `POST` | `/api/stop` | Stop active fill |
| `POST` | `/api/reset` | Return to idle when safe |
| `GET` | `/api/transactions` | JSON transaction history |
| `GET` | `/api/transactions.csv` | CSV transaction export |
| `GET` | `/api/modbus` | Human-readable Modbus register snapshot |

## WebSocket Real-Time Updates

The Expo app is WebSocket-ready and tries:

```text
ws://<device-ip>/ws
```

Current firmware does not yet expose `/ws`; the app automatically falls back to polling:

```text
GET /api/status every 1000 ms
```

When `/ws` is added, it should send the same JSON shape as `/api/status` whenever a value changes or at a fixed 250-1000 ms interval.

Recommended WebSocket message:

```json
{
  "state": "FILLING_FAST",
  "liveWeightKg": 16.25,
  "tareWeightKg": 5.00,
  "netWeightKg": 11.25,
  "targetWeightKg": 11.80,
  "ratePerKg": 250.00,
  "currentAmount": 2812.50,
  "nozzleEngaged": true,
  "cylinderPresent": true,
  "emergencyStopOk": true,
  "relays": [true, false, true, false, false, false]
}
```

## Real-Time Guidance

- Current display firmware polls the active RTU holding-register map, especially `0x0000..0x0017`, `0x0020..0x0025`, `0x0030..0x0035`, and diagnostics `0x0048..0x004B`.
- Suggested third-party HMI/SCADA polling interval: `250-1000 ms`. Avoid faster polling on 9600 baud RTU.
- Mobile app fallback polling interval: `1000 ms`.
- Avoid writing tare while filling is active. Firmware blocks HTTP zero-net during fill; Modbus clients should follow the same rule at the HMI layer.
- For the display-to-controller path, start/stop/reset use RTU command register `0x0017`. Older prototype TCP clients should still use HTTP for start/stop/reset.

## RTU Troubleshooting Notes

- If live weight/readiness update but START fails, check controller serial first. In one delivery debug session the real blocker was `Scale not calibrated`, not RS485.
- If controller serial `rtu` shows correct settings but logs CRC errors, check A/B wiring, display GPIO44/GPIO43, controller GPIO27/GPIO14, baud/parity, and whether display polling is interleaving with writes.
- If the controller was updated from older firmware, make sure saved calibration data is migrated. Current firmware treats a saved non-zero factor or valid two-point calibration as calibrated.
- Current display firmware uses a 300 ms RTU timeout, retry wrappers, and read-back confirmation after START. A single missed frame should show at most `RS485 unstable`; hard offline is only after the debounced communication state becomes offline.
- Display serial logs of `RTU busy req=01 03` during START indicate the write sequence is colliding with polling. Current display firmware serializes `startFill()` and `sendRecv()` using a recursive mutex and confirms fill state after the command.
