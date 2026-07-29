#!/bin/zsh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SKETCH_DIR="$REPO_ROOT/firmware/kc868_a6_lpg_controller"
BUILD_DIR="$SKETCH_DIR/build/ota"
FQBN="${FQBN:-esp32:esp32:esp32}"
LOCAL_ARDUINO_CLI="$REPO_ROOT/tools/local/arduino-cli-0.35.3"
IDE_ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
MAX_APP_BYTES=$((0x140000))

if [[ -n "${ARDUINO_CLI:-}" ]]; then
  CLI="$ARDUINO_CLI"
elif [[ -x "$LOCAL_ARDUINO_CLI" ]]; then
  CLI="$LOCAL_ARDUINO_CLI"
elif [[ -x "$IDE_ARDUINO_CLI" ]]; then
  CLI="$IDE_ARDUINO_CLI"
elif command -v arduino-cli >/dev/null 2>&1; then
  CLI="$(command -v arduino-cli)"
else
  echo "arduino-cli was not found. Set ARDUINO_CLI or install it first." >&2
  exit 1
fi

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

print "Building OTA application binary"
print "Sketch: $SKETCH_DIR"
print "FQBN:   $FQBN"
print "CLI:    $CLI"

"$CLI" compile \
  --fqbn "$FQBN" \
  --export-binaries \
  --output-dir "$BUILD_DIR" \
  "$SKETCH_DIR"

BIN_FILE="$BUILD_DIR/kc868_a6_lpg_controller.ino.bin"
if [[ ! -f "$BIN_FILE" ]]; then
  echo "Expected OTA binary was not produced: $BIN_FILE" >&2
  exit 1
fi

if stat -f%z "$BIN_FILE" >/dev/null 2>&1; then
  BIN_BYTES="$(stat -f%z "$BIN_FILE")"
else
  BIN_BYTES="$(stat -c%s "$BIN_FILE")"
fi

if (( BIN_BYTES > MAX_APP_BYTES )); then
  echo "OTA binary is too large: $BIN_BYTES bytes (slot limit $MAX_APP_BYTES)." >&2
  exit 1
fi

print "OTA binary: $BIN_FILE"
print "Size:       $BIN_BYTES / $MAX_APP_BYTES bytes"

if command -v shasum >/dev/null 2>&1; then
  shasum -a 256 "$BIN_FILE"
elif command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$BIN_FILE"
fi
