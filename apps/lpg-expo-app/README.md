# LPG Filling Expo App

Android-first operator/admin app for the KC868-A6 LPG filling controller.

## Run

```powershell
cd F:\Working\LPG-Filling-ESP\apps\lpg-expo-app
npm install
npm run android
```

The default controller URL is:

```text
http://192.168.0.108
```

Change it in the app and press `Connect` if the board gets a different IP.

## Real-Time Updates

The app first attempts:

```text
ws://<device-ip>/ws
```

If the firmware does not expose WebSocket yet, it falls back automatically to:

```text
GET /api/status
```

at a 1 second interval.

## Roles

- Operator: tare, zero net, weight/amount input, start/stop/reset.
- Admin: rate per kg, sales totals, period filters, recent history.
- Manufacturer: relay status and raw diagnostics.

## Protocol

See [Modbus and Real-Time Protocol](../../docs/modbus_and_realtime_protocol.md).
