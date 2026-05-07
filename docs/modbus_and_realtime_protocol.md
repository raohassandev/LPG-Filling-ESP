# LPG Controller Modbus and Real-Time Protocol

This document is the third-party integration contract for HMI, SCADA, gateway, and mobile app teams.

## Current Working Firmware Snapshot

This project now has two Modbus surfaces:

- Display-to-controller production path: Modbus RTU over RS485.
- Optional integration path: Modbus TCP on controller port `502`.

The display firmware uses the RTU map in `firmware/lpg_controller/include/ModbusRegisterMap.h`, with 0-based holding register addresses `0x0000..0x0047`. Do not use the older `0x1001` prototype map for the display firmware.

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

## Modbus TCP

The `0x1001` map below is the older Modbus TCP/prototype integration map. It is retained for third-party clients that already use it. The display firmware does not use this map.

Supported functions in the older TCP map:

- `0x03`: read holding registers
- `0x06`: write single holding register

Unsupported functions return Modbus exception `0x01`.
Unsupported addresses return exception `0x02`.
Read/write value errors return exception `0x03`.

### Register Map

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

Display START sequence:

1. Write target weight FLOAT32 to `0x0006..0x0007` using FC16.
2. Write rate FLOAT32 to `0x0008..0x0009` using FC16.
3. Write amount FLOAT32 to `0x000A..0x000B` using FC16.
4. Write command `1` to `0x0017` using FC06.

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

Readiness icons on the display cover only the visible safety inputs and stability. They do not prove calibration validity or transaction-log health; use controller serial `hx` and `start 12 250` when START is rejected.

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

- HMI/SCADA should poll Modbus register block `0x1001..0x1006` in a single request.
- Suggested Modbus polling interval: `250-1000 ms`.
- Mobile app fallback polling interval: `1000 ms`.
- Avoid writing tare while filling is active. Firmware blocks HTTP zero-net during fill; Modbus clients should follow the same rule at the HMI layer.
- For the display-to-controller path, start/stop/reset use RTU command register `0x0017`. Older prototype TCP clients should still use HTTP for start/stop/reset.

## RTU Troubleshooting Notes

- If live weight/readiness update but START fails, check controller serial first. In one delivery debug session the real blocker was `Scale not calibrated`, not RS485.
- If controller serial `rtu` shows correct settings but logs CRC errors, check A/B wiring, display GPIO44/GPIO43, controller GPIO27/GPIO14, baud/parity, and whether display polling is interleaving with writes.
- If the controller was updated from older firmware, make sure saved calibration data is migrated. Current firmware treats a saved non-zero factor or valid two-point calibration as calibrated.
- Display serial logs of `RTU busy req=01 03` during START indicate the write sequence is colliding with polling. Current display firmware serializes `startFill()` and `sendRecv()` using a recursive mutex.
