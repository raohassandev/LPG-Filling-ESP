#include <Arduino.h>
#include <SPIFFS.h>
#include <Wire.h>
#include <WiFi.h>

#include "BoardConfig.h"
#include "FillController.h"
#include "InputExpander.h"
#include "RelayBank.h"
#include "EventLog.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "WebPortal.h"
#include "WeightService.h"

namespace {
BoardConfig boardConfig;
StatusStore statusStore;
SettingsStore settingsStore;
EventLog eventLog;
RelayBank relayBank(boardConfig);
InputExpander inputExpander(boardConfig);
WeightService weightService;
FillController fillController(statusStore, relayBank, inputExpander, weightService, settingsStore, eventLog);
WebPortal webPortal(statusStore, fillController, weightService, settingsStore, eventLog);

void printStatusSnapshot() {
  const StatusSnapshot status = statusStore.snapshot();
  Serial.printf("[STATUS] state=%s weight=%.3f target=%.3f nozzle=%u cylinder=%u estop=%u reason=%s\n",
                status.stateLabel.c_str(), status.liveWeightKg, status.targetWeightKg, status.nozzleEngaged,
                status.cylinderPresent, status.emergencyStopOk, status.lastReasonCode.c_str());
}

void printSerialHelp() {
  Serial.println(F("[SERIAL] Commands:"));
  Serial.println(F("  help"));
  Serial.println(F("  status"));
  Serial.println(F("  sim <kg>"));
  Serial.println(F("  start <targetKg> <ratePerKg> [targetAmount]"));
  Serial.println(F("  stop"));
  Serial.println(F("  reset"));
  Serial.println(F("  mfgpin <4-8 digits>"));
}

void handleSerialCommand(const String& line) {
  String command = line;
  command.trim();
  if (command.isEmpty()) {
    return;
  }

  if (command == "help") {
    printSerialHelp();
    return;
  }

  if (command == "status") {
    printStatusSnapshot();
    return;
  }

  if (command == "stop") {
    Serial.printf("[SERIAL] stop: %s\n", fillController.stopFill("serial_stop") ? "ok" : "ignored");
    printStatusSnapshot();
    return;
  }

  if (command == "reset") {
    String reason;
    const bool ok = fillController.resetToIdle(reason);
    Serial.printf("[SERIAL] reset: %s (%s)\n", ok ? "ok" : "rejected", reason.c_str());
    printStatusSnapshot();
    return;
  }

  if (command.startsWith("mfgpin ")) {
    String reason;
    const String pin = command.substring(7);
    const bool ok = settingsStore.setManufacturingPin(pin, reason);
    eventLog.append(ok ? "INFO" : "WARN", ok ? "mfg_pin_updated" : "mfg_pin_rejected", reason);
    Serial.printf("[SERIAL] manufacturing PIN: %s (%s)\n", ok ? "updated" : "rejected", reason.c_str());
    return;
  }

  if (command.startsWith("sim ")) {
    const float value = command.substring(4).toFloat();
    weightService.setSimulatedWeightKg(value);
    Serial.printf("[SERIAL] simulated weight set to %.3f kg\n", value);
    printStatusSnapshot();
    return;
  }

  if (command.startsWith("start ")) {
    String payload = command.substring(6);
    payload.trim();
    const int firstSpace = payload.indexOf(' ');
    const int secondSpace = payload.indexOf(' ', firstSpace + 1);
    if (firstSpace < 0) {
      Serial.println(F("[SERIAL] usage: start <targetKg> <ratePerKg> [targetAmount]"));
      return;
    }

    const float targetKg = payload.substring(0, firstSpace).toFloat();
    const float ratePerKg =
        (secondSpace < 0 ? payload.substring(firstSpace + 1) : payload.substring(firstSpace + 1, secondSpace))
            .toFloat();
    float targetAmount = 0.0f;
    if (secondSpace >= 0) {
      targetAmount = payload.substring(secondSpace + 1).toFloat();
    }
    if (targetAmount <= 0.0f) {
      targetAmount = targetKg * ratePerKg;
    }

    String reason;
    const bool ok = fillController.startFill(targetKg, ratePerKg, targetAmount, reason);
    Serial.printf("[SERIAL] start: %s (%s)\n", ok ? "ok" : "rejected", reason.c_str());
    printStatusSnapshot();
    return;
  }

  Serial.printf("[SERIAL] unknown command: %s\n", command.c_str());
  printSerialHelp();
}

void pollSerialCommands() {
  static String buffer;
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (!buffer.isEmpty()) {
        handleSerialCommand(buffer);
        buffer = "";
      }
      continue;
    }

    if (buffer.length() < 120) {
      buffer += c;
    }
  }
}

void printBanner() {
  Serial.println();
  Serial.println(F("================================="));
  Serial.println(F("KC868-A6 LPG Controller"));
  Serial.println(F("Serial-first development baseline"));
  Serial.println(F("================================="));
}

void initFilesystem() {
  if (!SPIFFS.begin(true)) {
    Serial.println(F("[FS] SPIFFS mount failed"));
    return;
  }

  Serial.println(F("[FS] SPIFFS mounted"));
}

void initI2c() {
  Wire.begin(boardConfig.kI2cSdaPin, boardConfig.kI2cSclPin);
  Wire.setClock(100000);
  Serial.printf("[I2C] Initialized SDA=%u SCL=%u\n", boardConfig.kI2cSdaPin, boardConfig.kI2cSclPin);
}

void initWifi() {
  const SettingsSnapshot settings = settingsStore.snapshot();
  WiFi.mode(WIFI_AP);
  const bool started = WiFi.softAP(settings.apSsid.c_str(), settings.apPassword.c_str());
  if (!started) {
    Serial.println(F("[WiFi] Failed to start fallback AP"));
    return;
  }

  Serial.printf("[WiFi] AP started: %s\n", settings.apSsid.c_str());
  Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  printBanner();
  statusStore.begin();
  settingsStore.begin();
  statusStore.setBootReason("power_on");

  initFilesystem();
  eventLog.begin();
  initI2c();

  relayBank.begin();
  inputExpander.begin();
  weightService.begin();
  fillController.begin();
  initWifi();
  webPortal.begin();
  printSerialHelp();
  printStatusSnapshot();
  eventLog.append("INFO", "setup_complete", "System setup complete");

  Serial.println(F("[SYS] Setup complete"));
}

void loop() {
  inputExpander.poll();
  weightService.poll();
  fillController.tick();
  webPortal.handleClient();
  pollSerialCommands();
  delay(20);
}
