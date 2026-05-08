# Current Modbus RTU Map

This is the active display-to-controller Modbus RTU map. Addresses are 0-based PDU holding-register addresses.

## Link Settings

| Item | Value |
| --- | --- |
| Slave | `1` |
| Baud | `9600` |
| Format | `8N1` |
| Controller RX/TX | GPIO14 / GPIO27 |
| Display RX/TX | GPIO43 / GPIO44 |

## Core Registers

| Address | Name | Type | Access |
| --- | --- | --- | --- |
| `0x0000..0x0001` | Live weight kg | FLOAT32 | R |
| `0x0002..0x0003` | Tare weight kg | FLOAT32 | R/W |
| `0x0004..0x0005` | Net weight kg | FLOAT32 | R |
| `0x0006..0x0007` | Target weight kg | FLOAT32 | R/W |
| `0x0008..0x0009` | Rate per kg | FLOAT32 | R/W |
| `0x000A..0x000B` | Target amount | FLOAT32 | R/W |
| `0x000C..0x000D` | Current amount | FLOAT32 | R |
| `0x000E` | Fill state | UINT16 | R |
| `0x000F` | E-stop OK | UINT16 | R |
| `0x0010` | Cylinder present | UINT16 | R |
| `0x0011` | Nozzle engaged | UINT16 | R |
| `0x0012` | Weight stable | UINT16 | R |
| `0x0017` | Command | UINT16 | W |
| `0x0019` | RTU slave address | UINT16 | R/W |
| `0x001A..0x001B` | RTU baud | UINT32 | R/W |
| `0x001C` | RTU parity | UINT16 | R/W |
| `0x001D` | RTU stop bits | UINT16 | R/W |
| `0x0020..0x0025` | RTC Y/M/D/H/M/S | UINT16 | R/W |
| `0x0026..0x0027` | RTC Unix timestamp | UINT32 | R/W |
| `0x0030..0x0035` | Today stats | mixed | R |
| `0x0048` | Alarm code | UINT16 | R |
| `0x0049` | Alarm severity | UINT16 | R |
| `0x004A` | Readiness mask | UINT16 | R |
| `0x004B` | Blocker mask | UINT16 | R |
| `0x004C` | Scale initialized | UINT16 | R |
| `0x004D` | Scale read error | UINT16 | R |
| `0x004E` | Calibration valid | UINT16 | R |
| `0x004F` | Simulation active | UINT16 | R |
| `0x0050` | Alarm source | UINT16 | R |

Command register `0x0017`:

| Value | Command |
| --- | --- |
| `1` | Start fill |
| `2` | Stop fill |
| `3` | Reset |
| `4` | Zero net |

Do not use the old `0x1001` prototype map for the display.
