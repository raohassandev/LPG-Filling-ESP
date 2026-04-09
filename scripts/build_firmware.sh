#!/bin/zsh
set -euo pipefail

LOCAL_ARDUINO_CLI="/Users/israrulhaq/Desktop/DEV/LPG-Filling-ESP/tools/local/arduino-cli-0.35.3"
IDE_ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
SKETCH_DIR="/Users/israrulhaq/Desktop/DEV/LPG-Filling-ESP/firmware/kc868_a6_lpg_controller"
FQBN="${FQBN:-esp32:esp32:esp32}"

if [[ -x "$LOCAL_ARDUINO_CLI" ]]; then
  ARDUINO_CLI="$LOCAL_ARDUINO_CLI"
else
  ARDUINO_CLI="$IDE_ARDUINO_CLI"
fi

echo "Building sketch: $SKETCH_DIR"
echo "Using FQBN: $FQBN"
echo "Using arduino-cli: $ARDUINO_CLI"

"$ARDUINO_CLI" compile --fqbn "$FQBN" "$SKETCH_DIR"
