# Security Policy — KC868-A6 LPG Controller

## Default Credential Policy

There are **no default credentials**. On first boot the firmware generates a single `admin` account with a cryptographically random 8-character password printed once to the Serial console at 115200 baud. No operator or manufacturer accounts exist until the admin creates them.

If the Serial console is not accessible during first boot, connect a USB-to-serial adapter to the KC868-A6 UART0 pins and reboot.

## Password Hashing

Passwords are hashed using **SHA-256 with a per-user random 16-byte (hex) salt** via the ESP32 mbedTLS library:

```
hash = SHA256(salt + ":" + password)
```

Stored as 64 hex chars. Salts stored separately in NVS. Legacy djb2 hashes (8-char uppercase hex) are rejected — affected accounts must reset their password.

## Session Tokens

Session tokens are **128-bit cryptographically random** values generated via `esp_fill_random()` and encoded as 32 lowercase hex characters. Tokens are not derived from uptime, timestamp, or any predictable value. Sessions expire after 30 minutes of inactivity.

## Token Transport

Clients must send the token in the `Authorization: Bearer <token>` HTTP header. Query-string `?token=` is accepted as a backward-compatibility fallback but is deprecated and will be removed. Do not log URLs that contain tokens.

## Role Model

| Role | Level | Capabilities |
|------|-------|-------------|
| Operator | 1 | View status, start/stop fill, tare, view own transactions |
| Admin | 2 | All operator + manage users, settings, WiFi, logs, CSV export |
| Maintenance | 3 | All admin + calibration, relay control, Modbus config, RTC, dev-build sim |

## AP Password

The device AP (`LPG-XXXXXX`) password is derived from the last 3 bytes of the device MAC address at runtime: `LP<XX><XX><XX>`. No two devices share an AP password. The password is printed to Serial on boot and is stored in NVS — it can be changed via the admin interface.

## WiFi STA Credentials

No STA credentials are baked into firmware. SSID and password are entered via the admin portal and stored in NVS only. Never commit WiFi passwords to source control.

## CORS Policy

In the current release CORS is open (`Access-Control-Allow-Origin: *`) to support web-based tooling on a local network. All sensitive endpoints still require `Authorization: Bearer` — the CORS header only controls browser preflight, not the auth gate. For locked-down deployments, restrict origin by recompiling with the target domain.

## Production Build Flags

| Flag | Default | Purpose |
|------|---------|---------|
| `LPG_MODBUS_WRITES_ENABLED` | `0` | Enable Modbus HR write function codes |
| `LPG_DEV_BUILD` | undefined | Enable simulation endpoints (`/api/sim-*`) |

Do **not** define `LPG_DEV_BUILD` in production firmware.

## Vulnerability Reporting

Open an issue at https://github.com/raohassandev/LPG-Filling-ESP with the label `security`. For critical findings contact the maintainer directly before public disclosure.
