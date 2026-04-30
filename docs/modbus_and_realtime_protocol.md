# LPG Controller Modbus and Real-Time Protocol

This document is the third-party integration contract for HMI, SCADA, gateway, and mobile app teams.

## Device Network

- Default STA IP during testing: `192.168.0.108`
- HTTP API port: `80`
- Modbus TCP port: `502`
- Modbus unit id: accepted and echoed, currently not filtered
- Byte order: big-endian Modbus standard

## Modbus TCP

Supported functions:

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

Only tare weight is writable in the current prototype.

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
- Start/stop/reset should use HTTP API in the prototype because Modbus writes are intentionally limited to tare.

## RTU Notes

Modbus RTU is not enabled in the current firmware build. Before enabling RTU, confirm:

- RS485 port wiring on KC868-A6
- baud rate
- parity
- stop bits
- unit id
- HMI register base convention, either `0x1001` raw address or vendor display address `41002` style
