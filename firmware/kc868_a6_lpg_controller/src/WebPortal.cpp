#include "WebPortal.h"

#include <SPIFFS.h>

#include "BoardConfig.h"
#include "ModbusRegisterMap.h"

namespace {
String jsonBool(bool value) { return value ? "true" : "false"; }
}

WebPortal::WebPortal(StatusStore& statusStore, FillController& fillController, WeightService& weightService,
                     SettingsStore& settingsStore, EventLog& eventLog, TransactionLog& transactionLog,
                     RelayBank& relayBank)
    : statusStore_(statusStore),
      fillController_(fillController),
      weightService_(weightService),
      settingsStore_(settingsStore),
      eventLog_(eventLog),
      transactionLog_(transactionLog),
      relayBank_(relayBank) {}

void WebPortal::begin() {
  registerRoutes();
  server_.begin();
  Serial.println(F("[WEB] HTTP server started on port 80"));
}

void WebPortal::handleClient() { server_.handleClient(); }

void WebPortal::registerRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/health", HTTP_GET, [this]() { handleHealth(); });
  server_.on("/api/version", HTTP_GET, [this]() { handleVersion(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/api/weight", HTTP_GET, [this]() { handleWeight(); });
  server_.on("/api/settings", HTTP_GET, [this]() { handleSettings(); });
  server_.on("/api/settings", HTTP_POST, [this]() { handleUpdateSettings(); });
  server_.on("/api/tare", HTTP_POST, [this]() { handleSetTare(); });
  server_.on("/api/tare-zero", HTTP_POST, [this]() { handleZeroNetWeight(); });
  server_.on("/api/modbus", HTTP_GET, [this]() { handleModbusMap(); });
  server_.on("/api/logs", HTTP_GET, [this]() { handleLogs(); });
  server_.on("/api/transactions", HTTP_GET, [this]() { handleTransactions(); });
  server_.on("/api/transactions.csv", HTTP_GET, [this]() { handleTransactionsCsv(); });
  server_.on("/api/relay", HTTP_POST, [this]() { handleSetRelay(); });
  server_.on("/api/start", HTTP_POST, [this]() { handleStart(); });
  server_.on("/api/stop", HTTP_POST, [this]() { handleStop(); });
  server_.on("/api/reset", HTTP_POST, [this]() { handleReset(); });
  server_.on("/api/sim-weight", HTTP_POST, [this]() { handleSetSimWeight(); });
}

void WebPortal::handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    server_.send(404, "text/plain", "index.html not found");
    return;
  }
  server_.streamFile(file, "text/html");
  file.close();
}

void WebPortal::handleHealth() { server_.send(200, "application/json", "{\"ok\":true}"); }

void WebPortal::handleVersion() {
  String body = "{\"device\":\"";
  body += BoardConfig::kDeviceName;
  body += "\",\"firmwareVersion\":\"";
  body += BoardConfig::kFirmwareVersion;
  body += "\"}";
  server_.send(200, "application/json", body);
}

void WebPortal::handleStatus() { server_.send(200, "application/json", statusJson()); }

void WebPortal::handleWeight() {
  String body = "{\"weightKg\":";
  body += String(weightService_.liveWeightKg(), 3);
  body += ",\"tareWeightKg\":";
  body += String(statusStore_.snapshot().tareWeightKg, 3);
  body += ",\"netWeightKg\":";
  body += String(statusStore_.snapshot().netWeightKg, 3);
  body += "}";
  server_.send(200, "application/json", body);
}

void WebPortal::handleSettings() {
  const SettingsSnapshot settings = settingsStore_.snapshot();
  String body = "{";
  body += "\"apSsid\":\"" + settings.apSsid + "\",";
  body += "\"slowFillThreshold\":" + String(settings.slowFillThreshold, 2) + ",";
  body += "\"ratePerKg\":" + String(settings.ratePerKg, 2);
  body += "}";
  server_.send(200, "application/json", body);
}

void WebPortal::handleUpdateSettings() {
  const float ratePerKg = server_.arg("ratePerKg").toFloat();
  const bool ok = settingsStore_.setRatePerKg(ratePerKg);
  if (ok) {
    const StatusSnapshot status = statusStore_.snapshot();
    statusStore_.setTargets(status.targetWeightKg, status.targetAmount, ratePerKg);
    eventLog_.append("INFO", "rate_update", "Rate per kg updated from admin UI");
  }
  server_.send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"message\":\"invalid rate\"}");
}

void WebPortal::handleSetTare() {
  const StatusSnapshot status = statusStore_.snapshot();
  if (status.state == ProcessState::FillingFast || status.state == ProcessState::FillingSlow ||
      status.state == ProcessState::Settling) {
    server_.send(409, "application/json", "{\"ok\":false,\"message\":\"tare blocked during active fill\"}");
    return;
  }

  const float tareWeightKg = server_.arg("tareWeightKg").toFloat();
  statusStore_.setTareWeight(tareWeightKg);
  eventLog_.append("INFO", "tare_weight", "Operator tare weight set to " + String(tareWeightKg, 3) + " kg");
  server_.send(200, "application/json", "{\"ok\":true}");
}

void WebPortal::handleZeroNetWeight() {
  const StatusSnapshot status = statusStore_.snapshot();
  if (status.state == ProcessState::FillingFast || status.state == ProcessState::FillingSlow ||
      status.state == ProcessState::Settling) {
    server_.send(409, "application/json", "{\"ok\":false,\"message\":\"net zero blocked during active fill\"}");
    return;
  }

  statusStore_.setTareWeight(status.liveWeightKg);
  eventLog_.append("INFO", "net_zero", "Operator zeroed net weight from live scale");
  server_.send(200, "application/json", "{\"ok\":true}");
}

void WebPortal::handleModbusMap() {
  const StatusSnapshot status = statusStore_.snapshot();
  String body = "{\"scale\":\"kg_x100\",\"registers\":{";
  body += "\"0x1001\":{\"name\":\"liveWeight\",\"value\":" +
          String(ModbusRegisterMap::readHoldingRegister(ModbusRegisterMap::kLiveWeight, status)) + "},";
  body += "\"0x1002\":{\"name\":\"tareWeight\",\"value\":" +
          String(ModbusRegisterMap::readHoldingRegister(ModbusRegisterMap::kTareWeight, status)) + "},";
  body += "\"0x1003\":{\"name\":\"netWeight\",\"value\":" +
          String(ModbusRegisterMap::readHoldingRegister(ModbusRegisterMap::kNetWeight, status)) + "},";
  body += "\"0x1004\":{\"name\":\"fillingStatus\",\"value\":" +
          String(ModbusRegisterMap::readHoldingRegister(ModbusRegisterMap::kFillingStatus, status)) + "},";
  body += "\"0x1005\":{\"name\":\"targetWeight\",\"value\":" +
          String(ModbusRegisterMap::readHoldingRegister(ModbusRegisterMap::kTargetWeight, status)) + "},";
  body += "\"0x1006\":{\"name\":\"estopStatus\",\"value\":" +
          String(ModbusRegisterMap::readHoldingRegister(ModbusRegisterMap::kEstopStatus, status)) + "}";
  body += "}}";
  server_.send(200, "application/json", body);
}

void WebPortal::handleLogs() {
  server_.send(200, "text/plain", eventLog_.tail());
}

void WebPortal::handleTransactions() {
  server_.send(200, "application/json", transactionLog_.exportJson());
}

void WebPortal::handleTransactionsCsv() {
  server_.send(200, "text/csv", transactionLog_.exportCsv());
}

void WebPortal::handleSetRelay() {
  const StatusSnapshot status = statusStore_.snapshot();
  if (status.state == ProcessState::FillingFast || status.state == ProcessState::FillingSlow ||
      status.state == ProcessState::Settling) {
    server_.send(409, "application/json", "{\"ok\":false,\"message\":\"manual relay control blocked during fill\"}");
    return;
  }

  const int index = server_.arg("index").toInt();
  if (index < 0 || index >= BoardConfig::kRelayCount) {
    server_.send(400, "application/json", "{\"ok\":false,\"message\":\"invalid relay index\"}");
    return;
  }

  const String activeArg = server_.arg("active");
  const bool active = activeArg == "1" || activeArg == "true" || activeArg == "on";
  const bool ok = relayBank_.writeRelay(static_cast<uint8_t>(index), active);
  if (ok) {
    statusStore_.setRelay(static_cast<uint8_t>(index), active);
    eventLog_.append("WARN", "manual_relay", "Manual relay " + String(index + 1) + (active ? " on" : " off"));
  }

  server_.send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void WebPortal::handleStart() {
  const float targetWeightKg = server_.arg("targetWeightKg").toFloat();
  const float ratePerKg = server_.arg("ratePerKg").toFloat();
  float targetAmount = server_.arg("targetAmount").toFloat();
  if (targetAmount <= 0.0f) {
    targetAmount = targetWeightKg * ratePerKg;
  }
  String reason;
  const bool ok = fillController_.startFill(targetWeightKg, ratePerKg, targetAmount, reason);

  String body = "{\"ok\":";
  body += jsonBool(ok);
  body += ",\"message\":\"";
  body += reason;
  body += "\"}";
  server_.send(ok ? 200 : 400, "application/json", body);
}

void WebPortal::handleStop() {
  const bool ok = fillController_.stopFill("operator_stop");
  server_.send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true,\"message\":\"stopped\"}"
                                              : "{\"ok\":false,\"message\":\"no active fill\"}");
}

void WebPortal::handleReset() {
  String reason;
  const bool ok = fillController_.resetToIdle(reason);
  String body = "{\"ok\":";
  body += jsonBool(ok);
  body += ",\"message\":\"";
  body += reason;
  body += "\"}";
  server_.send(ok ? 200 : 400, "application/json", body);
}

void WebPortal::handleSetSimWeight() {
  weightService_.setSimulatedWeightKg(server_.arg("weightKg").toFloat());
  server_.send(200, "application/json", "{\"ok\":true}");
}

String WebPortal::statusJson() const {
  const StatusSnapshot status = statusStore_.snapshot();
  String body = "{";
  body += "\"state\":\"" + status.stateLabel + "\",";
  body += "\"bootReason\":\"" + status.bootReason + "\",";
  body += "\"weightKg\":" + String(status.liveWeightKg, 3) + ",";
  body += "\"liveWeightKg\":" + String(status.liveWeightKg, 3) + ",";
  body += "\"tareWeightKg\":" + String(status.tareWeightKg, 3) + ",";
  body += "\"netWeightKg\":" + String(status.netWeightKg, 3) + ",";
  body += "\"weightInitialized\":" + jsonBool(weightService_.initialized()) + ",";
  body += "\"weightReadError\":" + jsonBool(weightService_.readFailed()) + ",";
  body += "\"weightStable\":" + jsonBool(weightService_.stable()) + ",";
  body += "\"hx711DoutPin\":" + String(weightService_.doutPin()) + ",";
  body += "\"hx711DoutLevel\":" + String(weightService_.doutLevel()) + ",";
  body += "\"hx711SckPin\":" + String(weightService_.sckPin()) + ",";
  body += "\"hx711SckLevel\":" + String(weightService_.sckLevel()) + ",";
  body += "\"targetWeightKg\":" + String(status.targetWeightKg, 3) + ",";
  body += "\"targetAmount\":" + String(status.targetAmount, 2) + ",";
  body += "\"currentAmount\":" + String(status.netWeightKg * status.ratePerKg, 2) + ",";
  body += "\"ratePerKg\":" + String(status.ratePerKg, 2) + ",";
  body += "\"nozzleEngaged\":" + jsonBool(status.nozzleEngaged) + ",";
  body += "\"cylinderPresent\":" + jsonBool(status.cylinderPresent) + ",";
  body += "\"emergencyStopOk\":" + jsonBool(status.emergencyStopOk) + ",";
  body += "\"reasonCode\":\"" + status.lastReasonCode + "\",";
  body += "\"uptimeMs\":" + String(status.uptimeMs) + ",";
  body += "\"transactionCount\":" + String(transactionLog_.totalCount()) + ",";
  body += "\"modbus\":{\"liveWeight\":4097,\"tareWeight\":4098,\"netWeight\":4099,\"fillingStatus\":4100,\"targetWeight\":4101,\"estopStatus\":4102},";
  body += "\"relays\":[";

  for (uint8_t i = 0; i < 6; ++i) {
    if (i > 0) {
      body += ",";
    }
    body += jsonBool(status.relays[i]);
  }

  body += "],\"inputs\":[";
  for (uint8_t i = 0; i < 6; ++i) {
    if (i > 0) {
      body += ",";
    }
    body += jsonBool(status.inputs[i]);
  }
  body += "]}";
  return body;
}
