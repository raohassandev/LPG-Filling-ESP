#include <Arduino.h>
#include <SPIFFS.h>
#include <Wire.h>
#include <WiFi.h>

#include "BoardConfig.h"
#include "FillController.h"
#include "InputExpander.h"
#include "ModbusRegisterCache.h"
#include "ModbusRegisterMap.h"
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
#include "ModbusTcpService.h"
#include "ModbusRtuService.h"
#include "MqttService.h"
#include "SdService.h"
#include "ResourceMonitor.h"

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
LpgNetworkManager networkManager;
SdService sdService;
ModbusRegisterCache registerCache;
FillController fillController(statusStore, relayBank, inputExpander, weightService, settingsStore, eventLog,
                              transactionLog);
ModbusTcpService modbusTcpService(statusStore, settingsStore, fillController, transactionLog, rtcService, registerCache);
ModbusRtuService modbusRtuService(statusStore, settingsStore, fillController, transactionLog, rtcService, registerCache);
MqttService mqttService(networkManager, settingsStore, statusStore);
WebPortal webPortal(statusStore, fillController, weightService, settingsStore, eventLog, transactionLog, relayBank, authService, networkManager, rtcService, sdService, mqttService);

void printStatusSnapshot() {
  const StatusSnapshot status = statusStore.snapshot();
  Serial.printf("[STATUS] state=%s live=%.3f tare=%.3f net=%.3f target=%.3f rate=%.2f amount=%.2f nozzle=%u cylinder=%u estop=%u reason=%s\n",
                status.stateLabel.c_str(), status.liveWeightKg, status.tareWeightKg, status.netWeightKg,
                status.targetWeightKg, status.ratePerKg, status.netWeightKg * status.ratePerKg,
                status.nozzleEngaged, status.cylinderPresent, status.emergencyStopOk, status.lastReasonCode.c_str());
}

void printIoSnapshot() {
  const StatusSnapshot status = statusStore.snapshot();
  Serial.print(F("[IO] rawInputs="));
  for (uint8_t i = 0; i < BoardConfig::kInputCount; ++i) {
    if (i > 0) {
      Serial.print(',');
    }
    Serial.print(status.inputs[i] ? '1' : '0');
  }
  Serial.println();
  Serial.printf("[IO] mapping cylinder=IN%u(active-high) nozzle=IN%u(active-high) estop=IN%u(raw-tripped=%u)\n",
                BoardConfig::kInputCylinderPresent + 1, BoardConfig::kInputNozzleEngaged + 1,
                BoardConfig::kInputEmergencyStop + 1, BoardConfig::kInputEmergencyRawMeansTripped ? 1 : 0);
  Serial.printf("[IO] interpreted cylinder=%u nozzle=%u estopOk=%u\n",
                status.cylinderPresent ? 1 : 0, status.nozzleEngaged ? 1 : 0,
                status.emergencyStopOk ? 1 : 0);
}

void printSerialHelp() {
  Serial.println(F("[SERIAL] Commands:"));
  Serial.println(F("  help"));
  Serial.println(F("  status"));
  Serial.println(F("  io"));
  Serial.println(F("  weight"));
  Serial.println(F("  hx"));
  Serial.println(F("  tarew <emptyCylinderKg>"));
  Serial.println(F("  zeronet"));
  Serial.println(F("  sim <kg>"));
  Serial.println(F("  wifi <ssid> <password>"));
  Serial.println(F("  rtu"));
  Serial.println(F("  rtu on|off"));
  Serial.println(F("  rtu set <slaveId> <baud> <parity:0N/1E/2O> <stopBits:1/2>"));
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

  if (command == "io" || command == "inputs") {
    printIoSnapshot();
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

  if (command.startsWith("wifi ")) {
    String payload = command.substring(5);
    payload.trim();
    const int split = payload.indexOf(' ');
    if (split < 1) {
      Serial.println(F("[SERIAL] usage: wifi <ssid> <password>"));
      return;
    }
    const String ssid = payload.substring(0, split);
    const String pass = payload.substring(split + 1);
    if (!settingsStore.setWifi(ssid, pass)) {
      Serial.println(F("[SERIAL] wifi: rejected (SSID required, password 8+ chars)"));
      return;
    }
    networkManager.configureWifi(settingsStore.snapshot());
    networkManager.connectSTA(ssid, pass);
    Serial.printf("[SERIAL] wifi: saved, connecting to %s\n", ssid.c_str());
    return;
  }

  if (command == "rtu") {
    const ModbusRtuSettings rtu = settingsStore.rtuSnapshot();
    Serial.printf("[SERIAL] rtu: enabled=%u slave=%u baud=%lu data=8 parity=%u stop=%u rx=%u tx=%u de=%u\n",
                  rtu.enabled, rtu.slaveAddress, static_cast<unsigned long>(rtu.baudRate),
                  rtu.parity, rtu.stopBits, BoardConfig::kRtuRxPin, BoardConfig::kRtuTxPin,
                  BoardConfig::kRtuDePin);
    return;
  }

  if (command == "rtu on" || command == "rtu off") {
    ModbusRtuSettings rtu = settingsStore.rtuSnapshot();
    rtu.enabled = command.endsWith("on");
    if (!settingsStore.setModbusRtu(rtu)) {
      Serial.println(F("[SERIAL] rtu: save failed"));
      return;
    }
    Serial.println(F("[SERIAL] rtu: saved; restart required to apply enable/disable"));
    return;
  }

  if (command.startsWith("rtu set ")) {
    String payload = command.substring(8);
    payload.trim();
    const int p1 = payload.indexOf(' ');
    const int p2 = payload.indexOf(' ', p1 + 1);
    const int p3 = payload.indexOf(' ', p2 + 1);
    if (p1 < 1 || p2 < 0 || p3 < 0) {
      Serial.println(F("[SERIAL] usage: rtu set <slaveId> <baud> <parity:0N/1E/2O> <stopBits:1/2>"));
      return;
    }
    ModbusRtuSettings rtu = settingsStore.rtuSnapshot();
    rtu.enabled = true;
    rtu.slaveAddress = static_cast<uint8_t>(payload.substring(0, p1).toInt());
    rtu.baudRate = static_cast<uint32_t>(payload.substring(p1 + 1, p2).toInt());
    rtu.parity = static_cast<uint8_t>(payload.substring(p2 + 1, p3).toInt());
    rtu.stopBits = static_cast<uint8_t>(payload.substring(p3 + 1).toInt());
    if (!settingsStore.setModbusRtu(rtu)) {
      Serial.println(F("[SERIAL] rtu: rejected (slave 1-247, baud standard, parity 0-2, stop 1-2)"));
      return;
    }
    Serial.println(F("[SERIAL] rtu: saved; restart required to apply serial settings"));
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

  if (command == "zeronet") {
    const StatusSnapshot status = statusStore.snapshot();
    statusStore.setTareWeight(status.liveWeightKg);
    Serial.printf("[SERIAL] net weight zeroed at live weight %.3f kg\n", status.liveWeightKg);
    printStatusSnapshot();
    return;
  }

  if (command.startsWith("tare")) {
    weightService.requestTare();
    Serial.println(F("[SERIAL] tare started (non-blocking, completes in ~1.5 s via poll)"));
    return;
  }

  if (command.startsWith("cal ")) {
    const float factor = command.substring(4).toFloat();
    if (isfinite(factor) && factor != 0.0f) {
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


void updateOledStatus(bool force = false) {
  static unsigned long lastUpdateMs = 0;
  if (!force && millis() - lastUpdateMs < 1000) {
    return;
  }
  lastUpdateMs = millis();

  const StatusSnapshot status = statusStore.snapshot();
  const String staLine = networkManager.isSTAConnected()
                             ? "STA: " + networkManager.staIP()
                             : "STA: CONNECTING";
  oledDisplay.showLines("LPG CONTROLLER", staLine, "AP: " + networkManager.apIP(),
                        "STATE: " + status.stateLabel);
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
  ResourceMonitor::instance().begin();
  ResourceMonitor::instance().setModbusWritesEnabled(ModbusRegisterMap::modbusWritesEnabled());
  networkManager.begin(settingsStore.snapshot());
  networkManager.connectSTA(settingsStore.snapshot().staSsid, settingsStore.snapshot().staPassword);
  // mDNS is managed entirely by NetworkManager — started/restarted via poll() on every connection
  sdService.begin();
  webPortal.begin();
  modbusTcpService.begin();
  modbusRtuService.begin();
  mqttService.begin();
  updateOledStatus(true);
  printSerialHelp();
  printStatusSnapshot();
  eventLog.append("INFO", "setup_complete", "System setup complete");

  Serial.println(F("[SYS] Setup complete"));
}

void loop() {
  const uint32_t loopStartUs = micros();

  // ── Fast control path ────────────────────────────────────────────────────
  inputExpander.poll();
  weightService.poll();  // non-blocking: returns immediately when HX711 DOUT not ready
  statusStore.setWeightStable(weightService.stable());
  statusStore.setScaleHealth(weightService.initialized(), weightService.readFailed(),
                             weightService.calibrationValid(), weightService.simActive());

  const ProcessState prevState = statusStore.snapshot().state;
  fillController.tick();
  const ProcessState newState  = statusStore.snapshot().state;

  // MQTT state propagated before Modbus so kHR_MqttConnected reads correctly
  const bool mqttOk = mqttService.isConnected();
  modbusTcpService.setMqttConnected(mqttOk);
  modbusRtuService.setMqttConnected(mqttOk);

  // ── Register cache: fast process/IO/comms/alarm update ───────────────────
  registerCache.updateFast(statusStore.snapshot(), settingsStore, mqttOk);

  // ── Modbus hot path — reads from RAM cache, must run early ───────────────
  modbusRtuService.handleClient();
  modbusTcpService.handleClient();

  // ── Medium path (every ~500 ms): RTC + resource monitor ──────────────────
  static uint32_t lastMediumMs = 0;
  {
    const uint32_t nowMs = millis();
    if (nowMs - lastMediumMs >= 500) {
      lastMediumMs = nowMs;
      registerCache.updateRtc(rtcService.getTime());
      registerCache.updateResource(ResourceMonitor::instance().snapshot());
      networkManager.poll();
    }
  }

  // ── Slow path (every ~5 s): transaction statistics ────────────────────────
  static uint32_t lastStatsMs = 0;
  {
    const uint32_t nowMs = millis();
    if (nowMs - lastStatsMs >= 5000) {
      lastStatsMs = nowMs;
      registerCache.updateStats(transactionLog);
    }
  }

  // ── Fill state transition side-effects ───────────────────────────────────
  if (prevState != ProcessState::Complete && newState == ProcessState::Complete) {
    const TransactionRecord rec = transactionLog.getLatestTransaction();
    if (rec.id > 0) {
      mqttService.publishTransaction(rec);
      const uint8_t mode = settingsStore.snapshot().storageMode;
      if ((mode == 1 || mode == 2) && sdService.isReady()) {
        sdService.appendTransaction(rec);
      }
    }
    // Refresh stats cache immediately after a transaction completes
    registerCache.updateStats(transactionLog);
    lastStatsMs = millis();
  }
  if (prevState != ProcessState::Fault && newState == ProcessState::Fault) {
    const StatusSnapshot s = statusStore.snapshot();
    mqttService.publishAlert("fault", s.lastReasonCode);
  }

  // ── Low-priority background services ─────────────────────────────────────
  webPortal.handleClient();
  mqttService.loop();
  pollSerialCommands();
  updateOledStatus();

  ResourceMonitor::instance().sample(static_cast<uint16_t>(WiFi.status()),
                                     networkManager.isSTAConnected() ? static_cast<int16_t>(WiFi.RSSI()) : 0,
                                     mqttOk ? 1 : 0);
  ResourceMonitor::instance().recordLoop(static_cast<uint32_t>(micros() - loopStartUs));

  // Yield to WiFi/BT stack; do not use delay(5) as it adds 5 ms minimum latency.
  delay(1);
}
