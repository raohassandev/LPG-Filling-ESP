# MQTT Integration

MQTT is optional and client-only. The ESP32 controller is not an MQTT broker, and local filling must continue to work when MQTT or WiFi is unavailable.

## Build Flag

MQTT code is compiled only when:

```cpp
LPG_MQTT_ENABLED=1
```

Without that flag, the firmware keeps the API shape but MQTT publish operations are no-ops.

## Topics

Current topic shape:

```text
<topicPrefix>/<stationId>/<controllerId>/status
<topicPrefix>/<stationId>/<controllerId>/transaction
<topicPrefix>/<stationId>/<controllerId>/alert
<topicPrefix>/<stationId>/<controllerId>/lwt
```

Default `topicPrefix` is `lpg/controller` unless changed in settings.

## Test Endpoint

```text
POST /api/mqtt/test
```

Response includes:

- `compiled`
- `enabled`
- `connected`
- `topic`
- `message`

## Status Payload

Status publishes include:

- `deviceType`
- `stationId`
- `controllerId`
- `siteName`
- `nozzleId`
- state and weight fields
- readiness booleans
- `alarmCode`
- `alarmSeverity`
- `readinessMask`
- `blockerMask`
- `uptimeSec`

Remote start/stop commands are intentionally not enabled by default.
