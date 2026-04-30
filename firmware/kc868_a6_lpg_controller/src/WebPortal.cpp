#include "WebPortal.h"

#include <SPIFFS.h>

#include "BoardConfig.h"

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
  body += "}";
  server_.send(200, "application/json", body);
}

void WebPortal::handleSettings() {
  const SettingsSnapshot settings = settingsStore_.snapshot();
  String body = "{";
  body += "\"apSsid\":\"" + settings.apSsid + "\",";
  body += "\"slowFillThreshold\":" + String(settings.slowFillThreshold, 2);
  body += "}";
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
  body += "\"weightInitialized\":" + jsonBool(weightService_.initialized()) + ",";
  body += "\"weightReadError\":" + jsonBool(weightService_.readFailed()) + ",";
  body += "\"weightStable\":" + jsonBool(weightService_.stable()) + ",";
  body += "\"hx711DoutPin\":" + String(weightService_.doutPin()) + ",";
  body += "\"hx711DoutLevel\":" + String(weightService_.doutLevel()) + ",";
  body += "\"hx711SckPin\":" + String(weightService_.sckPin()) + ",";
  body += "\"hx711SckLevel\":" + String(weightService_.sckLevel()) + ",";
  body += "\"targetWeightKg\":" + String(status.targetWeightKg, 3) + ",";
  body += "\"targetAmount\":" + String(status.targetAmount, 2) + ",";
  body += "\"ratePerKg\":" + String(status.ratePerKg, 2) + ",";
  body += "\"nozzleEngaged\":" + jsonBool(status.nozzleEngaged) + ",";
  body += "\"cylinderPresent\":" + jsonBool(status.cylinderPresent) + ",";
  body += "\"emergencyStopOk\":" + jsonBool(status.emergencyStopOk) + ",";
  body += "\"reasonCode\":\"" + status.lastReasonCode + "\",";
  body += "\"uptimeMs\":" + String(status.uptimeMs) + ",";
  body += "\"transactionCount\":" + String(transactionLog_.totalCount()) + ",";
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
