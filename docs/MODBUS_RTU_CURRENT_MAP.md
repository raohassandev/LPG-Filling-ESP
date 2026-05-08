# Current Modbus RTU Map

This is a compact pointer to the active display-to-controller Modbus RTU map. The full source of truth is:

- Firmware source: `firmware/lpg_controller/include/ModbusRegisterMap.h`
- Register behavior: `firmware/lpg_controller/src/ModbusRegisterMap.cpp`
- User manual table: `docs/user/MODBUS_PROTOCOL.md`
- Controller webpage: `/modbus-map` or `/modbus.html`

Addresses are 0-based PDU holding-register addresses. Some Modbus tools display Holding Register addresses as 40001 + PDU address.

Old 0x1001 map is legacy only and must not be used for ESP32-S3 display, Haiwell HMI, Modbus Poll verification, or current SCADA integration.

## Link Settings

| Item | Value |
| --- | --- |
| Slave | `1` |
| Baud | `9600` |
| Format | `8N1` |
| Controller RX/TX | GPIO14 / GPIO27 |
| Display RX/TX | GPIO43 / GPIO44 |

## Key Registers

| Decimal Address | Hex Address | Description | Data Type | Access | Number of Registers |
|---:|---|---|---|---|---:|
| 0000 | 0x0000 | Live Weight | Float32 | R | 2 |
| 0006 | 0x0006 | Target Weight | Float32 | RW | 2 |
| 0008 | 0x0008 | Rate Per Kg | Float32 | RW | 2 |
| 0010 | 0x000A | Target Amount | Float32 | RW | 2 |
| 0012 | 0x000C | Current Amount | Float32 | R | 2 |
| 0014 | 0x000E | Fill State | UINT16 | R | 1 |
| 0023 | 0x0017 | Command | UINT16 | W | 1 |
| 0024 | 0x0018 | Device ID | UINT16 | R | 1 |
| 0072 | 0x0048 | Alarm Code | UINT16 | R | 1 |
| 0074 | 0x004A | Readiness Mask | UINT16 | R | 1 |
| 0075 | 0x004B | Blocker Mask | UINT16 | R | 1 |
| 0081 | 0x0051 | Application Load Percent | Float32 | R | 2 |
| 0087 | 0x0057 | Heap Total Memory | UINT32 | R | 2 |
| 0089 | 0x0059 | Heap Free Memory | UINT32 | R | 2 |
| 0107 | 0x006B | ESP32 Internal Chip Temperature | Float32 | R | 2 |
| 0109 | 0x006D | WiFi RSSI | INT16 | R | 1 |
| 0110 | 0x006E | WiFi Status | UINT16 | R | 1 |
| 0114 | 0x0072 | Modbus RTU Request Count | UINT32 | R | 2 |
| 0116 | 0x0074 | Modbus RTU Error Count | UINT32 | R | 2 |
| 0118 | 0x0076 | Controller Heartbeat Counter | UINT32 | R | 2 |

Device ID register `0x0018` must return `0xA601`. If a signed Modbus Poll view shows `-23039`, that is the signed representation of `0xA601`, not an error.

Command register `0x0017`:

| Value | Command |
| --- | --- |
| `1` | Start fill |
| `2` | Stop fill |
| `3` | Reset |
| `4` | Zero net |

Production builds reject Modbus writes unless writes are explicitly enabled. Prototype builds use `-DLPG_PROTOTYPE_BUILD=1` to enable preset and command writes.
