# KC868-A6 LPG Controller Firmware

This sketch is the custom firmware baseline for the LPG filling project.

Current scope:

- verified KC868-A6 I2C addresses and relay behavior modeled in code
- safe boot path with all relays forced to the inactive state
- fallback AP mode for device-hosted UI
- serial-first diagnostics
- status API and minimal HMI
- simulated weight path for early logic and UI testing
- serial command interface for start, stop, reset, and simulated weight
- persistent AP and slow-fill settings load path
- local append-only event log exposed by API

Hardware integration reference:

- [Load Cell and HX711 Connection Guide](/Users/israrulhaq/Desktop/DEV/LPG-Filling-ESP/docs/load_cell_hx711_connection_guide.md)

Not complete yet:

- real load-cell integration
- RTC management
- transaction logging
- finalized input truth table
- production security
- finalized network mode policy

Suggested compile target:

- FQBN: `esp32:esp32:esp32`
- preferred CLI in this repo: `tools/local/arduino-cli-0.35.3`

Serial commands after flashing:

- `help`
- `status`
- `sim 5.25`
- `start 11.8 250`
- `stop`
- `reset`

Useful HTTP endpoints:

- `GET /api/status`
- `GET /api/settings`
- `GET /api/logs`
- `POST /api/start`
- `POST /api/stop`
- `POST /api/reset`
- `POST /api/sim-weight`

Recommended next implementation steps:

1. verify input truth table on real hardware
2. verify relay truth table on real hardware
3. add persistent settings and transaction log
4. replace simulated weight with real measurement service
5. harden process state machine and fault handling
