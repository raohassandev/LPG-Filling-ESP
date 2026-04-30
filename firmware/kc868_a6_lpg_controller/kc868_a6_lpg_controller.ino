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
#include "RtcService.h"
#include "TransactionLog.h"
#include "AuthService.h"
#include "InputTruthTable.h"
#include "NetworkManager.h"
#include "OledDisplay.h"

namespace {
BoardConfig boardConfig;
StatusStore statusStore;
SettingsStore settingsStore;
EventLog eventLog;
RelayBank relayBank(boardConfig);
InputExpander inputExpander(boardConfig);
WeightService weightService;
RtcService rtcService;
TransactionLog transactionLog(rtcService);
AuthService authService;
OledDisplay oledDisplay(boardConfig);
FillController fillController(statusStore, relayBank, inputExpander, weightService, settingsStore, eventLog,
                              transactionLog);
WebPortal webPortal(statusStore, fillController, weightService, settingsStore, eventLog, transactionLog, relayBank);

String activeStaIp;

void printStatusSnapshot() {
  const StatusSnapshot status = statusStore.snapshot();
  Serial.printf("[STATUS] state=%s live=%.3f tare=%.3f net=%.3f target=%.3f rate=%.2f amount=%.2f nozzle=%u cylinder=%u estop=%u reason=%s\n",
                status.stateLabel.c_str(), status.liveWeightKg, status.tareWeightKg, status.netWeightKg,
                status.targetWeightKg, status.ratePerKg, status.netWeightKg * status.ratePerKg,
                status.nozzleEngaged, status.cylinderPresent, status.emergencyStopOk, status.lastReasonCode.c_str());
}

void printSerialHelp() {
  Serial.println(F("[SERIAL] Commands:"));
  Serial.println(F("  help"));
  Serial.println(F("  status"));
  Serial.println(F("  weight"));
  Serial.println(F("  hx"));
  Serial.println(F("  tarew <emptyCylinderKg>"));
  Serial.println(F("  sim <kg>"));
  Serial.println(F("  start <targetKg> <ratePerKg> [targetAmount]"));
  Serial.println(F("  stop"));
  Serial.println(F("  reset"));
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

  if (command == "weight" || command == "hx") {
    Serial.printf("[WEIGHT] initialized=%u readError=%u stable=%u kg=%.3f doutPin=%u doutLevel=%d sckPin=%u sckLevel=%d\n",
                  weightService.initialized(), weightService.readFailed(), weightService.stable(),
                  weightService.liveWeightKg(), weightService.doutPin(), weightService.doutLevel(),
                  weightService.sckPin(), weightService.sckLevel());
    Serial.printf("[WEIGHT] raw=%ld tareOffset=%ld calibration=%.2f\n", weightService.lastRawValue(),
                  weightService.tareOffsetRaw(), weightService.calibrationFactor());
    return;
  }

  if (command.startsWith("sim ")) {
    const float weightKg = command.substring(4).toFloat();
    weightService.setSimulatedWeightKg(weightKg);
    Serial.printf("[SERIAL] simulated weight set to %.3f kg\n", weightKg);
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

  if (command.startsWith("tarew ")) {
    const float tareWeightKg = command.substring(6).toFloat();
    statusStore.setTareWeight(tareWeightKg);
    Serial.printf("[SERIAL] empty cylinder tare weight set to %.3f kg\n", tareWeightKg);
    printStatusSnapshot();
    return;
  }

  if (command.startsWith("tare")) {
    weightService.tare();
    Serial.println(F("[SERIAL] scale tare completed"));
    printStatusSnapshot();
    return;
  }

  if (command.startsWith("cal ")) {
    const float factor = command.substring(4).toFloat();
    if (factor > 0) {
      weightService.setCalibrationFactor(factor);
      Serial.printf("[SERIAL] calibration factor set to %.2f\n", factor);
    } else {
      Serial.println(F("[SERIAL] usage: cal <factor>"));
    }
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
  WiFi.mode(WIFI_AP_STA);

  const bool apStarted = WiFi.softAP(settings.apSsid.c_str(), settings.apPassword.c_str());
  if (!apStarted) {
    Serial.println(F("[WiFi] Failed to start fallback AP"));
  } else {
    Serial.printf("[WiFi] Fallback AP started: %s\n", settings.apSsid.c_str());
    Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  }

  Serial.printf("[WiFi] Connecting STA to SSID: %s\n", settings.staSsid.c_str());
  WiFi.begin(settings.staSsid.c_str(), settings.staPassword.c_str());

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    activeStaIp = WiFi.localIP().toString();
    Serial.printf("[WiFi] STA connected: %s\n", settings.staSsid.c_str());
    Serial.printf("[WiFi] STA IP: %s\n", activeStaIp.c_str());
  } else {
    activeStaIp = "";
    Serial.println(F("[WiFi] STA failed; fallback AP remains available"));
  }
}

void updateOledStatus(bool force = false) {
  static unsigned long lastUpdateMs = 0;
  if (!force && millis() - lastUpdateMs < 1000) {
    return;
  }
  lastUpdateMs = millis();

  const StatusSnapshot status = statusStore.snapshot();
  const String staLine = activeStaIp.isEmpty() ? "STA: CONNECTING" : "STA: " + activeStaIp;
  oledDisplay.showLines("LPG CONTROLLER", staLine, "AP: " + WiFi.softAPIP().toString(),
                        "STATE: " + status.stateLabel);
}

void pollWifi() {
  static wl_status_t lastStatus = WL_IDLE_STATUS;
  const wl_status_t currentStatus = WiFi.status();
  if (currentStatus == lastStatus) {
    return;
  }
  lastStatus = currentStatus;

  if (currentStatus == WL_CONNECTED) {
    activeStaIp = WiFi.localIP().toString();
    Serial.printf("[WiFi] STA IP: %s\n", activeStaIp.c_str());
  } else {
    activeStaIp = "";
    Serial.printf("[WiFi] STA status changed: %u\n", static_cast<uint8_t>(currentStatus));
  }
  updateOledStatus(true);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  printBanner();
  statusStore.begin();
  settingsStore.begin();
  statusStore.setBootReason("power_on");
  statusStore.setTargets(0.0f, 0.0f, settingsStore.snapshot().ratePerKg);

  initFilesystem();
  eventLog.begin();
  initI2c();
  oledDisplay.begin();
  oledDisplay.showLines("LPG CONTROLLER", "BOOTING", "PLEASE WAIT", "");

  relayBank.begin();
  inputExpander.begin();
  weightService.begin();
  rtcService.begin();
  transactionLog.begin();
  authService.begin();
  fillController.begin();
  initWifi();
  webPortal.begin();
  updateOledStatus(true);
  printSerialHelp();
  printStatusSnapshot();
  eventLog.append("INFO", "setup_complete", "System setup complete");

  Serial.println(F("[SYS] Setup complete"));
}

void loop() {
  inputExpander.poll();
  weightService.poll();
  fillController.tick();
  pollWifi();
  webPortal.handleClient();
  pollSerialCommands();
  updateOledStatus();
  delay(20);
}
