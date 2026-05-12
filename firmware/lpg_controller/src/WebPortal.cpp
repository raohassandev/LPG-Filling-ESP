#include "WebPortal.h"

#include <SPIFFS.h>

#include "BoardConfig.h"
#include "ModbusRegisterMap.h"
#include "ResourceMonitor.h"

namespace {
String jsonBool(bool value) { return value ? "true" : "false"; }
String jsonStr(const String& s) {
  // Minimal JSON string escape: replaces " and backslash
  String out = "\"";
  for (size_t i = 0; i < s.length(); i++) {
    const char c = s[i];
    if (c == '"')       out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else                out += c;
  } // end for
  out += "\"";
  return out;
}
}

WebPortal::WebPortal(StatusStore& statusStore, FillController& fillController, WeightService& weightService,
                     SettingsStore& settingsStore, EventLog& eventLog, TransactionLog& transactionLog,
                     RelayBank& relayBank, AuthService& authService, LpgNetworkManager& networkManager,
                     RtcService& rtcService, SdService& sdService, MqttService& mqttService)
    : statusStore_(statusStore),
      fillController_(fillController),
      weightService_(weightService),
      settingsStore_(settingsStore),
      eventLog_(eventLog),
      transactionLog_(transactionLog),
      relayBank_(relayBank),
      authService_(authService),
      networkManager_(networkManager),
      rtcService_(rtcService),
      sdService_(sdService),
      mqttService_(mqttService) {}

void WebPortal::begin() {
  // Collect Authorization header so requireAuth() can read Bearer tokens
  const char* collectHeaders[] = { "Authorization" };
  server_.collectHeaders(collectHeaders, 1);

  registerRoutes();
  server_.begin();
  Serial.println(F("[WEB] HTTP server started on port 80"));

#if LPG_WEBSOCKET_ENABLED
  wsServer_.begin();
  wsServer_.onEvent([](uint8_t, WStype_t type, uint8_t*, size_t) {
    if (type == WStype_CONNECTED) Serial.println(F("[WS] Client connected"));
    if (type == WStype_DISCONNECTED) Serial.println(F("[WS] Client disconnected"));
  });
  Serial.println(F("[WS] WebSocket server started on port 81"));
#else
  Serial.println(F("[WS] Disabled (LPG_WEBSOCKET_ENABLED=0)"));
#endif
}

void WebPortal::handleClient() {
  server_.handleClient();
#if LPG_WEBSOCKET_ENABLED
  wsServer_.loop();
  broadcastStatus();
#endif
}

void WebPortal::broadcastStatus() {
#if LPG_WEBSOCKET_ENABLED
  if (millis() - lastBroadcastMs_ < 200) return;
  lastBroadcastMs_ = millis();
  if (wsServer_.connectedClients() == 0) return;
  // Broadcast minimal non-sensitive health payload to all WS clients (no auth on WS).
  // Clients that need full status must use authenticated GET /api/status.
  const StatusSnapshot s = statusStore_.snapshot();
  String payload = "{\"state\":\"" + s.stateLabel + "\""
                 + ",\"readyToFill\":" + jsonBool(weightService_.initialized() &&
                                                  !weightService_.readFailed() &&
                                                  weightService_.stable() &&
                                                  weightService_.calibrationValid() &&
                                                  s.emergencyStopOk &&
                                                  s.cylinderPresent &&
                                                  s.nozzleEngaged &&
                                                  s.state != ProcessState::Fault)
                 + ",\"emergencyStopOk\":" + jsonBool(s.emergencyStopOk)
                 + "}";
  wsServer_.broadcastTXT(payload);
#endif
}

void WebPortal::sendCorsHeaders() {
  server_.sendHeader("Access-Control-Allow-Origin", "*");
  server_.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server_.sendHeader("Access-Control-Allow-Headers", "*");
}

void WebPortal::sendJson(int code, const String& body) {
  sendCorsHeaders();
  server_.send(code, "application/json", body);
}

void WebPortal::registerRoutes() {
  server_.onNotFound([this]() {
    if (server_.method() == HTTP_OPTIONS) {
      sendCorsHeaders();
      server_.send(204);
    } else {
      sendJson(404, "{\"ok\":false,\"message\":\"not found\"}");
    }
  });
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/wifi.html", HTTP_GET, [this]() {
    File file = SPIFFS.open("/wifi.html", "r");
    if (!file) { sendJson(404, "{\"ok\":false,\"message\":\"wifi.html not found\"}"); return; }
    sendCorsHeaders(); server_.streamFile(file, "text/html"); file.close();
  });
  server_.on("/modbus.html", HTTP_GET, [this]() {
    File file = SPIFFS.open("/modbus.html", "r");
    if (!file) { sendJson(404, "{\"ok\":false,\"message\":\"modbus.html not found\"}"); return; }
    sendCorsHeaders(); server_.streamFile(file, "text/html"); file.close();
  });
  server_.on("/modbus-map", HTTP_GET, [this]() {
    File file = SPIFFS.open("/modbus.html", "r");
    if (!file) { sendJson(404, "{\"ok\":false,\"message\":\"modbus.html not found\"}"); return; }
    sendCorsHeaders(); server_.streamFile(file, "text/html"); file.close();
  });
  server_.on("/api/health", HTTP_GET, [this]() { handleHealth(); });
  server_.on("/api/version", HTTP_GET, [this]() { handleVersion(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/api/weight", HTTP_GET, [this]() { handleWeight(); });
  server_.on("/api/settings", HTTP_GET, [this]() { handleSettings(); });
  server_.on("/api/settings", HTTP_POST, [this]() { handleUpdateSettings(); });
  server_.on("/api/tare",      HTTP_POST, [this]() { handleSetTare(); });
  server_.on("/api/tare-zero", HTTP_POST, [this]() { handleZeroNetWeight(); });
  server_.on("/api/tare-hw",   HTTP_POST, [this]() { handleHwTare(); });
  server_.on("/api/modbus", HTTP_GET, [this]() { handleModbusMap(); });
  server_.on("/api/logs", HTTP_GET, [this]() { handleLogs(); });
  server_.on("/api/transactions", HTTP_GET, [this]() { handleTransactions(); });
  server_.on("/api/transactions.csv", HTTP_GET, [this]() { handleTransactionsCsv(); });
  server_.on("/api/relay", HTTP_POST, [this]() { handleSetRelay(); });
  server_.on("/api/relay/test-pulse", HTTP_POST, [this]() { handleRelayTestPulse(); });
  server_.on("/api/start", HTTP_POST, [this]() { handleStart(); });
  server_.on("/api/stop", HTTP_POST, [this]() { handleStop(); });
  server_.on("/api/reset", HTTP_POST, [this]() { handleReset(); });
#ifdef LPG_DEV_BUILD
  server_.on("/api/sim-weight",  HTTP_POST, [this]() { handleSetSimWeight(); });
  server_.on("/api/sim-clear",   HTTP_POST, [this]() { handleClearSim(); });
  server_.on("/api/sim-inputs",       HTTP_POST, [this]() { handleSetSimInputs(); });
  server_.on("/api/sim-inputs-clear", HTTP_POST, [this]() { handleClearSimInputs(); });
#endif
  server_.on("/api/calibrate",   HTTP_POST, [this]() { handleCalibrate(); });
  server_.on("/api/login",   HTTP_POST, [this]() { handleLogin(); });
  server_.on("/api/logout",  HTTP_POST, [this]() { handleLogout(); });
  server_.on("/api/wifi",    HTTP_GET,  [this]() { handleGetWifi(); });
  server_.on("/api/wifi",    HTTP_POST, [this]() { handleSetWifi(); });
  server_.on("/api/wifi/scan", HTTP_GET, [this]() { handleWifiScan(); });
  server_.on("/api/network", HTTP_GET,  [this]() { handleGetNetwork(); });
  server_.on("/api/io/live",  HTTP_GET,  [this]() { handleGetIoLive(); });
  server_.on("/api/users",         HTTP_GET,    [this]() { handleListUsers(); });
  server_.on("/api/users",         HTTP_POST,   [this]() { handleCreateUser(); });
  server_.on("/api/users/update",  HTTP_POST,   [this]() { handleUpdateUser(); });
  server_.on("/api/users/delete",  HTTP_POST,   [this]() { handleDeleteUser(); });
  server_.on("/api/system",        HTTP_GET,    [this]() { handleGetSystem(); });
  server_.on("/api/system",        HTTP_POST,   [this]() { handleSetSystem(); });
  server_.on("/api/mqtt",          HTTP_GET,    [this]() { handleGetMqtt(); });
  server_.on("/api/mqtt",          HTTP_POST,   [this]() { handleSetMqtt(); });
  server_.on("/api/mqtt/test",     HTTP_POST,   [this]() { handleMqttTest(); });
  server_.on("/api/config/export", HTTP_GET,    [this]() { handleConfigExport(); });
  server_.on("/api/config/import", HTTP_POST,   [this]() { handleConfigImport(); });
  server_.on("/api/commissioning", HTTP_GET,    [this]() { handleGetCommissioning(); });
  server_.on("/api/commissioning", HTTP_POST,   [this]() { handleSetCommissioning(); });
  server_.on("/api/stats",         HTTP_GET,    [this]() { handleGetStats(); });
  server_.on("/api/time",          HTTP_GET,    [this]() { handleGetTime(); });
  server_.on("/api/time",          HTTP_POST,   [this]() { handleSetTime(); });
  server_.on("/api/modbus-rtu",    HTTP_GET,    [this]() { handleGetModbusRtu(); });
  server_.on("/api/modbus-rtu",    HTTP_POST,   [this]() { handleSetModbusRtu(); });
  server_.on("/api/sd/months",        HTTP_GET, [this]() { handleGetSdMonths(); });
  server_.on("/api/sd/transactions",  HTTP_GET, [this]() { handleGetSdTransactions(); });
}

void WebPortal::handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    sendJson(404, "{\"ok\":false,\"message\":\"index.html not found\"}");
    return;
  }
  sendCorsHeaders();
  server_.streamFile(file, "text/html");
  file.close();
}

void WebPortal::handleHealth() { sendJson(200, "{\"ok\":true}"); }

void WebPortal::handleVersion() {
  String body = "{\"device\":\"";
  body += BoardConfig::kDeviceName;
  body += "\",\"firmwareVersion\":\"";
  body += BoardConfig::kFirmwareVersion;
  body += "\"}";
  sendJson(200, body);
}

void WebPortal::handleStatus() {
  if (!requireAuth(UserRole::Operator)) return;
  sendJson(200, statusJson());
}

void WebPortal::handleWeight() {
  if (!requireAuth(UserRole::Operator)) return;
  const StatusSnapshot snap = statusStore_.snapshot();
  const bool twoPoint = weightService_.hasTwoPoints();
  String body = "{";
  body += "\"weightKg\":"    + String(weightService_.liveWeightKg(), 3) + ",";
  body += "\"tareWeightKg\":" + String(snap.tareWeightKg, 3) + ",";
  body += "\"netWeightKg\":"  + String(snap.netWeightKg, 3) + ",";
  body += "\"rawValue\":"     + String(weightService_.lastRawValue()) + ",";
  body += "\"tareRawValue\":" + String(weightService_.tareOffsetRaw()) + ",";
  body += "\"calFactor\":"    + String(weightService_.calibrationFactor(), 2) + ",";
  body += "\"calMode\":\""    + String(twoPoint ? "two-point" : "single") + "\",";
  if (twoPoint) {
    body += "\"calLowRaw\":"  + String(weightService_.calLowPoint().rawAbs)  + ",";
    body += "\"calLowKg\":"   + String(weightService_.calLowPoint().kg, 3)   + ",";
    body += "\"calHighRaw\":" + String(weightService_.calHighPoint().rawAbs) + ",";
    body += "\"calHighKg\":"  + String(weightService_.calHighPoint().kg, 3)  + ",";
  }
  body += "\"simActive\":"    + String(weightService_.simActive() ? "true" : "false") + ",";
  body += "\"initialized\":"  + String(weightService_.initialized() ? "true" : "false") + ",";
  body += "\"readError\":"    + String(weightService_.readFailed() ? "true" : "false") + ",";
  body += "\"doutLevel\":"    + String(weightService_.doutLevel()) + ",";
  body += "\"sckLevel\":"     + String(weightService_.sckLevel());
  body += "}";
  sendJson(200, body);
}

void WebPortal::handleSettings() {
  if (!requireAuth(UserRole::Operator)) return;
  const SettingsSnapshot settings = settingsStore_.snapshot();
  String body = "{";
  body += "\"apSsid\":\"" + settings.apSsid + "\",";
  body += "\"slowFillThreshold\":" + String(settings.slowFillThreshold, 2) + ",";
  body += "\"ratePerKg\":" + String(settings.ratePerKg, 2) + ",";
  body += "\"storageMode\":" + String(settings.storageMode) + ",";
  body += "\"sdReady\":" + jsonBool(sdService_.isReady()) + ",";
  body += "\"sdTotalKb\":" + String(sdService_.totalKb()) + ",";
  body += "\"sdFreeKb\":" + String(sdService_.freeKb());
  body += "}";
  sendJson(200, body);
}

void WebPortal::handleUpdateSettings() {
  // Allow admin OR operators delegated canSetRate
  if (!requireAuth(UserRole::Operator)) return;
  if (!authService_.canCurrentUserSetRate()) {
    sendJson(403, "{\"ok\":false,\"message\":\"rate setting not permitted for this account\"}");
    return;
  }

  bool ok = true;
  String msg;

  if (server_.hasArg("ratePerKg")) {
    const float ratePerKg = server_.arg("ratePerKg").toFloat();
    ok = settingsStore_.setRatePerKg(ratePerKg);
    if (ok) {
      const StatusSnapshot status = statusStore_.snapshot();
      statusStore_.setTargets(status.targetWeightKg, status.targetAmount, ratePerKg);
      eventLog_.append("INFO", "rate_update", "Rate per kg updated from admin UI");
    } else {
      msg = "invalid rate";
    }
  }

  if (ok && server_.hasArg("slowFillThreshold")) {
    const float threshold = server_.arg("slowFillThreshold").toFloat();
    ok = settingsStore_.setSlowFillThreshold(threshold);
    if (ok) {
      eventLog_.append("INFO", "slow_fill_threshold", "Slow fill threshold set to " + String(threshold, 2));
    } else {
      msg = "slowFillThreshold must be 0.80–0.99";
    }
  }

  if (ok && server_.hasArg("storageMode")) {
    const uint8_t mode = (uint8_t)server_.arg("storageMode").toInt();
    ok = settingsStore_.setStorageMode(mode);
    if (!ok) msg = "storageMode must be 0 (SPIFFS), 1 (SD), or 2 (Both)";
  }

  sendJson(ok ? 200 : 400, ok ? "{\"ok\":true}" : "{\"ok\":false,\"message\":\"" + msg + "\"}");
}

void WebPortal::handleSetTare() {
  if (!requireAuth(UserRole::Operator)) return;
  const StatusSnapshot status = statusStore_.snapshot();
  if (status.state == ProcessState::FillingFast || status.state == ProcessState::FillingSlow ||
      status.state == ProcessState::Settling) {
    sendJson(409, "{\"ok\":false,\"message\":\"tare blocked during active fill\"}");
    return;
  }

  const float tareWeightKg = server_.arg("tareWeightKg").toFloat();
  statusStore_.setTareWeight(tareWeightKg);
  eventLog_.append("INFO", "tare_weight", "Operator tare weight set to " + String(tareWeightKg, 3) + " kg");
  sendJson(200, "{\"ok\":true}");
}

void WebPortal::handleHwTare() {
  if (!requireAuth(UserRole::Maintenance)) return;
  const StatusSnapshot status = statusStore_.snapshot();
  if (status.state == ProcessState::FillingFast || status.state == ProcessState::FillingSlow ||
      status.state == ProcessState::Settling) {
    sendJson(409, "{\"ok\":false,\"message\":\"tare blocked during active fill\"}");
    return;
  }
  weightService_.tare();
  statusStore_.setTareWeight(0.0f);
  eventLog_.append("INFO", "hw_tare", "Hardware tare completed");
  sendJson(200, "{\"ok\":true,\"rawOffset\":" + String(weightService_.tareOffsetRaw()) + "}");
}

void WebPortal::handleZeroNetWeight() {
  if (!requireAuth(UserRole::Operator)) return;
  const StatusSnapshot status = statusStore_.snapshot();
  if (status.state == ProcessState::FillingFast || status.state == ProcessState::FillingSlow ||
      status.state == ProcessState::Settling) {
    sendJson(409, "{\"ok\":false,\"message\":\"net zero blocked during active fill\"}");
    return;
  }

  statusStore_.setTareWeight(status.liveWeightKg);
  eventLog_.append("INFO", "net_zero", "Operator zeroed net weight from live scale");
  sendJson(200, "{\"ok\":true}");
}

void WebPortal::handleModbusMap() {
  if (!requireAuth(UserRole::Maintenance)) return;
  using namespace ModbusRegisterMap;
  const StatusSnapshot status = statusStore_.snapshot();
  const RtcTime rtcTime = rtcService_.getTime();
  // Expose raw 16-bit register values for all 25 holding registers (PDU 0x0000–0x0018).
  // Addresses shown as Modbus Poll display numbers (40001 + PDU addr).
  char buf[32];
  String body = "{\"port\":502,\"protocol\":\"ModbusTCP\","
                "\"hrBase\":0,\"hrCount\":" + String(kHR_Count) + ","
                "\"coilBase\":1,\"coilCount\":" + String(kCoil_Count) + ","
                "\"diBase\":10001,\"diCount\":" + String(kDI_Count) + ","
                "\"registers\":{";
  for (uint16_t i = 0; i < kHR_Count; i++) {
    const uint16_t val = readHR(i, status, transactionLog_, settingsStore_, rtcTime, mqttService_.isConnected());
    snprintf(buf, sizeof(buf), "\"0x%04X\":%u", i, val);
    body += (i > 0 ? "," : "");
    body += buf;
  }
  body += "},\"coils\":{";
  for (uint16_t i = 0; i < kCoil_Count; i++) {
    snprintf(buf, sizeof(buf), "\"0x%04X\":%u", i, readCoil(i, status));
    body += (i > 0 ? "," : "");
    body += buf;
  }
  body += "},\"discreteInputs\":{";
  for (uint16_t i = 0; i < kDI_Count; i++) {
    snprintf(buf, sizeof(buf), "\"0x%04X\":%u", i, readDI(i, status));
    body += (i > 0 ? "," : "");
    body += buf;
  }
  body += "}}";
  sendJson(200, body);
}

void WebPortal::handleLogs() {
  if (!requireAuth(UserRole::Admin)) return;
  sendCorsHeaders();
  server_.send(200, "text/plain", eventLog_.tail());
}

void WebPortal::handleTransactions() {
  if (!requireAuth(UserRole::Operator)) return;
  const String filterUser = server_.arg("username");
  // Operators can only see their own history; Admin+ can see all or filter
  if (!filterUser.isEmpty()) {
    sendJson(200, transactionLog_.exportJsonForUser(filterUser));
  } else if (authService_.hasPermission(UserRole::Admin)) {
    sendJson(200, transactionLog_.exportJson());
  } else {
    // Operator with no filter: show own history
    sendJson(200, transactionLog_.exportJsonForUser(authService_.currentUsername()));
  }
}

void WebPortal::handleTransactionsCsv() {
  if (!requireAuth(UserRole::Admin)) return;
  sendCorsHeaders();
  server_.send(200, "text/csv", transactionLog_.exportCsv());
}

void WebPortal::handleSetRelay() {
  if (!requireAuth(UserRole::Maintenance)) return;
  const StatusSnapshot status = statusStore_.snapshot();
  if (status.state == ProcessState::FillingFast || status.state == ProcessState::FillingSlow ||
      status.state == ProcessState::Settling) {
    sendJson(409, "{\"ok\":false,\"message\":\"manual relay control blocked during fill\"}");
    return;
  }

  const int index = server_.arg("index").toInt();
  if (index < 0 || index >= BoardConfig::kRelayCount) {
    sendJson(400, "{\"ok\":false,\"message\":\"invalid relay index\"}");
    return;
  }

  const String activeArg = server_.arg("active");
  const bool active = activeArg == "1" || activeArg == "true" || activeArg == "on";
  const bool ok = relayBank_.writeRelay(static_cast<uint8_t>(index), active);
  if (ok) {
    statusStore_.setRelay(static_cast<uint8_t>(index), active);
    eventLog_.append("WARN", "manual_relay", "Manual relay " + String(index + 1) + (active ? " on" : " off"));
  }

  sendJson(ok ? 200 : 500, ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void WebPortal::handleRelayTestPulse() {
  if (!requireAuth(UserRole::Maintenance)) return;
  const StatusSnapshot status = statusStore_.snapshot();
  if (status.state == ProcessState::FillingFast || status.state == ProcessState::FillingSlow ||
      status.state == ProcessState::Settling || status.state == ProcessState::Validating) {
    sendJson(409, "{\"ok\":false,\"message\":\"relay pulse blocked during active fill\"}");
    return;
  }

  const int index = server_.arg("relay").toInt() - 1;
  uint16_t durationMs = static_cast<uint16_t>(server_.arg("durationMs").toInt());
  if (index < 0 || index >= BoardConfig::kRelayCount) {
    sendJson(400, "{\"ok\":false,\"message\":\"invalid relay; use 1-6\"}");
    return;
  }
  if (durationMs == 0) durationMs = 500;
  if (durationMs > 2000) {
    sendJson(400, "{\"ok\":false,\"message\":\"durationMs max is 2000\"}");
    return;
  }

  const uint8_t relay = static_cast<uint8_t>(index);
  bool ok = relayBank_.writeAllSafe();
  for (uint8_t i = 0; i < BoardConfig::kRelayCount; ++i) statusStore_.setRelay(i, false);
  ok = ok && relayBank_.writeRelay(relay, true);
  statusStore_.setRelay(relay, ok);
  if (ok) delay(durationMs);
  relayBank_.writeAllSafe();
  for (uint8_t i = 0; i < BoardConfig::kRelayCount; ++i) statusStore_.setRelay(i, false);
  eventLog_.append("WARN", "relay_test", "Relay " + String(relay + 1) + " pulse " + String(durationMs) + "ms");

  String body = "{\"ok\":";
  body += jsonBool(ok);
  body += ",\"relay\":" + String(relay + 1) + ",\"durationMs\":" + String(durationMs) + "}";
  sendJson(ok ? 200 : 500, body);
}

void WebPortal::handleStart() {
  if (!requireAuth(UserRole::Operator)) return;
  const float targetWeightKg = server_.arg("targetWeightKg").toFloat();
  const float ratePerKg = server_.arg("ratePerKg").toFloat();
  float targetAmount = server_.arg("targetAmount").toFloat();
  if (targetAmount <= 0.0f) {
    targetAmount = targetWeightKg * ratePerKg;
  }
  String reason;
  const bool ok = fillController_.startFill(targetWeightKg, ratePerKg, targetAmount, reason,
                                             authService_.currentUsername());

  String body = "{\"ok\":";
  body += jsonBool(ok);
  body += ",\"message\":\"";
  body += reason;
  body += "\"}";
  sendJson(ok ? 200 : 400, body);
}

void WebPortal::handleStop() {
  if (!requireAuth(UserRole::Operator)) return;
  const bool ok = fillController_.stopFill("operator_stop");
  sendJson(ok ? 200 : 400, ok ? "{\"ok\":true,\"message\":\"stopped\"}"
                              : "{\"ok\":false,\"message\":\"no active fill\"}");
}

void WebPortal::handleReset() {
  if (!requireAuth(UserRole::Operator)) return;
  String reason;
  const bool ok = fillController_.resetToIdle(reason);
  String body = "{\"ok\":";
  body += jsonBool(ok);
  body += ",\"message\":\"";
  body += reason;
  body += "\"}";
  sendJson(ok ? 200 : 400, body);
}

#ifdef LPG_DEV_BUILD
void WebPortal::handleSetSimWeight() {
  if (!requireAuth(UserRole::Maintenance)) return;
  weightService_.setSimulatedWeightKg(server_.arg("weightKg").toFloat());
  sendJson(200, "{\"ok\":true}");
}

void WebPortal::handleClearSim() {
  if (!requireAuth(UserRole::Maintenance)) return;
  weightService_.clearSimulation();
  sendJson(200, "{\"ok\":true}");
}

void WebPortal::handleSetSimInputs() {
  if (!requireAuth(UserRole::Maintenance)) return;
  const bool cylinder = server_.arg("cylinderPresent") == "1" || server_.arg("cylinderPresent") == "true";
  const bool nozzle   = server_.arg("nozzleEngaged")   == "1" || server_.arg("nozzleEngaged")   == "true";
  fillController_.setSimInputs(cylinder, nozzle);
  sendJson(200, "{\"ok\":true}");
}

void WebPortal::handleClearSimInputs() {
  if (!requireAuth(UserRole::Maintenance)) return;
  fillController_.clearSimInputs();
  sendJson(200, "{\"ok\":true}");
}
#endif // LPG_DEV_BUILD

void WebPortal::handleCalibrate() {
  if (!requireAuth(UserRole::Maintenance)) return;
  const String factorArg  = server_.arg("factor");
  const String knownKgArg = server_.arg("knownKg");
  const String pointArg   = server_.arg("point");

  const String calUser = authService_.currentUsername();

  if (!factorArg.isEmpty()) {
    const float factor = factorArg.toFloat();
    if (factor == 0.0f) { sendJson(400, "{\"ok\":false,\"message\":\"factor cannot be zero\"}"); return; }
    const float oldFactor = weightService_.calibrationFactor();
    weightService_.clearCalPoints();
    weightService_.setCalibrationFactor(factor);
    eventLog_.append("CAL", "cal_factor_set",
      "user=" + calUser + " mode=single oldFactor=" + String(oldFactor, 2) +
      " newFactor=" + String(factor, 2));
    sendJson(200, "{\"ok\":true,\"mode\":\"single\",\"calFactor\":" + String(factor, 2) + "}");

  } else if (!knownKgArg.isEmpty()) {
    const float knownKg = knownKgArg.toFloat();
    if (knownKg <= 0.0f) { sendJson(400, "{\"ok\":false,\"message\":\"knownKg must be positive\"}"); return; }

    if (!pointArg.isEmpty()) {
      const int pt = pointArg.toInt();
      if (pt != 1 && pt != 2) { sendJson(400, "{\"ok\":false,\"message\":\"point must be 1 or 2\"}"); return; }
      weightService_.setCalPoint(static_cast<uint8_t>(pt), knownKg);
      const bool active = weightService_.hasTwoPoints();
      const long rawAbs = pt == 1 ? weightService_.calLowPoint().rawAbs : weightService_.calHighPoint().rawAbs;
      eventLog_.append("CAL", "cal_point_set",
        "user=" + calUser + " mode=two-point point=" + String(pt) +
        " knownKg=" + String(knownKg, 3) + " rawAbs=" + String(rawAbs) +
        " twoPointActive=" + String(active ? "true" : "false"));
      String body = "{\"ok\":true,\"mode\":\"" + String(active ? "two-point" : "two-point-partial") + "\"";
      body += ",\"point\":"    + String(pt);
      body += ",\"rawAbs\":"   + String(rawAbs);
      body += ",\"kg\":"       + String(knownKg, 3);
      body += ",\"twoPointActive\":" + String(active ? "true" : "false") + "}";
      sendJson(200, body);

    } else {
      const long raw  = weightService_.lastRawValue();
      const long tare = weightService_.tareOffsetRaw();
      const float factor = static_cast<float>(raw - tare) / knownKg;
      if (factor == 0.0f) { sendJson(400, "{\"ok\":false,\"message\":\"raw equals tare — place known weight first\"}"); return; }
      const float oldFactor = weightService_.calibrationFactor();
      weightService_.clearCalPoints();
      weightService_.setCalibrationFactor(factor);
      eventLog_.append("CAL", "cal_single_point",
        "user=" + calUser + " mode=single knownKg=" + String(knownKg, 3) +
        " raw=" + String(raw) + " tare=" + String(tare) +
        " oldFactor=" + String(oldFactor, 2) + " newFactor=" + String(factor, 2));
      sendJson(200, "{\"ok\":true,\"mode\":\"single\",\"calFactor\":" + String(factor, 2)
               + ",\"raw\":" + String(raw) + ",\"tare\":" + String(tare) + "}");
    }
  } else {
    sendJson(400, "{\"ok\":false,\"message\":\"provide factor=, knownKg=, or knownKg= with point=1|2\"}");
  }
}

bool WebPortal::requireAuth(UserRole minRole) {
  // Accept Authorization: Bearer <token> header; fall back to ?token= query arg for backward compat
  String token;
  if (server_.hasHeader("Authorization")) {
    String auth = server_.header("Authorization");
    if (auth.startsWith("Bearer ")) {
      token = auth.substring(7);
      token.trim();
    }
  }
  if (token.isEmpty()) {
    token = server_.arg("token");
  }
  if (token.isEmpty()) {
    sendJson(401, "{\"ok\":false,\"message\":\"token required\"}");
    return false;
  }
  if (!authService_.sessionValid()) {
    sendJson(401, "{\"ok\":false,\"message\":\"session expired\"}");
    return false;
  }
  if (authService_.getCurrentSession().sessionId != token) {
    sendJson(401, "{\"ok\":false,\"message\":\"invalid token\"}");
    return false;
  }
  if (!authService_.hasPermission(minRole)) {
    sendJson(403, "{\"ok\":false,\"message\":\"insufficient role\"}");
    return false;
  }
  authService_.refreshSession();
  return true;
}

void WebPortal::handleLogin() {
  const String username = server_.arg("username");
  const String password = server_.arg("password");
  if (authService_.login(username, password)) {
    const SessionInfo session = authService_.getCurrentSession();
    String roleStr;
    switch (session.role) {
      case UserRole::Admin:       roleStr = "admin"; break;
      case UserRole::Maintenance: roleStr = "manufacturer"; break;
      default:                    roleStr = "operator"; break;
    }
    String body = "{\"ok\":true";
    body += ",\"token\":\""      + session.sessionId + "\"";
    body += ",\"role\":\""       + roleStr + "\"";
    body += ",\"username\":\""   + authService_.currentUsername() + "\"";
    body += ",\"canSetRate\":"   + String(authService_.canCurrentUserSetRate() ? "true" : "false");
    body += "}";
    sendJson(200, body);
  } else {
    sendJson(401, "{\"ok\":false,\"message\":\"invalid credentials\"}");
  }
}

void WebPortal::handleGetWifi() {
  if (!requireAuth(UserRole::Operator)) return;
  const SettingsSnapshot s = settingsStore_.snapshot();
  const bool connected = networkManager_.isSTAConnected();
  const String staIP   = connected ? networkManager_.staIP() : "";
  String body = "{";
  body += "\"staEnabled\":" + jsonBool(s.staEnabled) + ",";
  body += "\"apEnabled\":" + jsonBool(s.apEnabled) + ",";
  body += "\"autoSwitch\":" + jsonBool(s.wifiAutoSwitch) + ",";
  body += "\"dhcp\":" + jsonBool(s.staDhcp) + ",";
  body += "\"staticIp\":" + jsonStr(s.staStaticIp) + ",";
  body += "\"gateway\":" + jsonStr(s.staGateway) + ",";
  body += "\"subnet\":" + jsonStr(s.staSubnet) + ",";
  body += "\"dns1\":" + jsonStr(s.staDns1) + ",";
  body += "\"dns2\":" + jsonStr(s.staDns2) + ",";
  body += "\"connected\":" + jsonBool(connected) + ",";
  body += "\"staSSID\":" + jsonStr(networkManager_.staSSID()) + ",";
  body += "\"staIP\":" + jsonStr(staIP) + ",";
  body += "\"apSSID\":" + jsonStr(networkManager_.apSSID()) + ",";
  body += "\"apIP\":" + jsonStr(networkManager_.apIP()) + ",";
  body += "\"networks\":[";
  for (uint8_t i = 0; i < s.wifiCount; i++) {
    if (i) body += ",";
    body += "{\"index\":" + String(i) + ",";
    body += "\"ssid\":" + jsonStr(s.wifiSsid[i]) + ",";
    body += "\"enabled\":" + jsonBool(s.wifiEnabled[i]) + ",";
    body += "\"priority\":" + String(i + 1) + "}";
  }
  body += "]}";
  sendJson(200, body);
}

void WebPortal::handleSetWifi() {
  if (!requireAuth(UserRole::Operator)) return;
  if (server_.hasArg("staEnabled") || server_.hasArg("apEnabled") || server_.hasArg("autoSwitch")) {
    const SettingsSnapshot s = settingsStore_.snapshot();
    const bool staEnabled = server_.hasArg("staEnabled") ? server_.arg("staEnabled") == "1" : s.staEnabled;
    const bool apEnabled = server_.hasArg("apEnabled") ? server_.arg("apEnabled") == "1" : s.apEnabled;
    const bool autoSwitch = server_.hasArg("autoSwitch") ? server_.arg("autoSwitch") == "1" : s.wifiAutoSwitch;
    if (!settingsStore_.setWifiFlags(staEnabled, apEnabled, autoSwitch)) {
      sendJson(500, "{\"ok\":false,\"message\":\"WiFi mode save failed\"}");
      return;
    }
    networkManager_.configureWifi(settingsStore_.snapshot());
  }
  if (server_.hasArg("dhcp") || server_.hasArg("staticIp") || server_.hasArg("gateway") ||
      server_.hasArg("subnet") || server_.hasArg("dns1") || server_.hasArg("dns2")) {
    const SettingsSnapshot s = settingsStore_.snapshot();
    const bool dhcp = server_.hasArg("dhcp") ? (server_.arg("dhcp") == "1" || server_.arg("dhcp") == "true") : s.staDhcp;
    const String ip = server_.hasArg("staticIp") ? server_.arg("staticIp") : s.staStaticIp;
    const String gw = server_.hasArg("gateway") ? server_.arg("gateway") : s.staGateway;
    const String sn = server_.hasArg("subnet") ? server_.arg("subnet") : s.staSubnet;
    const String dns1 = server_.hasArg("dns1") ? server_.arg("dns1") : s.staDns1;
    const String dns2 = server_.hasArg("dns2") ? server_.arg("dns2") : s.staDns2;
    if (!settingsStore_.setStaIpConfig(dhcp, ip, gw, sn, dns1, dns2)) {
      sendJson(500, "{\"ok\":false,\"message\":\"IP config save failed\"}");
      return;
    }
    networkManager_.configureWifi(settingsStore_.snapshot());
  }
  if (server_.hasArg("remove")) {
    if (!settingsStore_.removeWifiNetwork(static_cast<uint8_t>(server_.arg("remove").toInt()))) {
      sendJson(400, "{\"ok\":false,\"message\":\"Invalid network index\"}");
      return;
    }
    networkManager_.configureWifi(settingsStore_.snapshot());
    sendJson(200, "{\"ok\":true,\"message\":\"WiFi network removed\"}");
    return;
  }
  const String ssid = server_.hasArg("ssid") ? server_.arg("ssid") : server_.arg("staSsid");
  const String pass = server_.hasArg("password") ? server_.arg("password") : server_.arg("staPassword");
  if (ssid.isEmpty() && pass.isEmpty()) {
    sendJson(200, "{\"ok\":true,\"message\":\"WiFi settings saved\"}");
    return;
  }
  const bool enabled = !server_.hasArg("enabled") || server_.arg("enabled") == "1";
  if (!settingsStore_.upsertWifiNetwork(ssid, pass, enabled)) {
    sendJson(400, "{\"ok\":false,\"message\":\"SSID required and password must be 8+ chars\"}");
    return;
  }
  eventLog_.append("INFO", "wifi_update", "WiFi STA credentials updated");
  networkManager_.configureWifi(settingsStore_.snapshot());
  networkManager_.connectSTA(ssid, pass);
  sendJson(200, "{\"ok\":true,\"message\":\"WiFi credentials saved. Reconnecting...\"}");
}

void WebPortal::handleWifiScan() {
  if (!requireAuth(UserRole::Operator)) return;
  const int found = WiFi.scanNetworks(false, true);
  String body = "{\"networks\":[";
  for (int i = 0; i < found; i++) {
    if (i) body += ",";
    body += "{\"ssid\":" + jsonStr(WiFi.SSID(i)) + ",";
    body += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    body += "\"secure\":" + jsonBool(WiFi.encryptionType(i) != WIFI_AUTH_OPEN) + "}";
  }
  body += "]}";
  WiFi.scanDelete();
  sendJson(200, body);
}

void WebPortal::handleGetNetwork() {
  if (!requireAuth(UserRole::Operator)) return;
  const SettingsSnapshot s = settingsStore_.snapshot();
  String body = "{";
  body += "\"staConnected\":" + String(networkManager_.isSTAConnected() ? "true" : "false") + ",";
  body += "\"staSSID\":\""    + networkManager_.staSSID() + "\",";
  body += "\"staIP\":\""      + networkManager_.staIP()   + "\",";
  body += "\"dhcp\":"         + jsonBool(s.staDhcp) + ",";
  body += "\"staticIp\":"     + jsonStr(s.staStaticIp) + ",";
  body += "\"gateway\":"      + jsonStr(s.staGateway) + ",";
  body += "\"subnet\":"       + jsonStr(s.staSubnet) + ",";
  body += "\"dns1\":"         + jsonStr(s.staDns1) + ",";
  body += "\"dns2\":"         + jsonStr(s.staDns2) + ",";
  body += "\"apSSID\":\""     + networkManager_.apSSID()  + "\",";
  body += "\"apIP\":\""       + networkManager_.apIP()    + "\"";
  body += "}";
  sendJson(200, body);
}

void WebPortal::handleGetIoLive() {
  if (!requireAuth(UserRole::Maintenance)) return;
  const StatusSnapshot s = statusStore_.snapshot();

  String body = "{\"ok\":true,\"rawInputs\":[";
  for (uint8_t i = 0; i < BoardConfig::kInputCount; ++i) {
    if (i > 0) body += ",";
    body += jsonBool(s.inputs[i]);
  }
  body += "],\"mapping\":{";
  body += "\"cylinder\":{\"channel\":" + String(BoardConfig::kInputCylinderPresent) + ",\"activeHigh\":true},";
  body += "\"nozzle\":{\"channel\":" + String(BoardConfig::kInputNozzleEngaged) + ",\"activeHigh\":true},";
  body += "\"estop\":{\"channel\":" + String(BoardConfig::kInputEmergencyStop)
       + ",\"rawMeansTripped\":" + jsonBool(BoardConfig::kInputEmergencyRawMeansTripped) + "}";
  body += "},\"interpreted\":{";
  body += "\"cylinderPresent\":" + jsonBool(s.cylinderPresent) + ",";
  body += "\"nozzleEngaged\":" + jsonBool(s.nozzleEngaged) + ",";
  body += "\"emergencyStopOk\":" + jsonBool(s.emergencyStopOk);
  body += "}}";
  sendJson(200, body);
}

void WebPortal::handleLogout() {
  String token;
  if (server_.hasHeader("Authorization")) {
    String auth = server_.header("Authorization");
    if (auth.startsWith("Bearer ")) { token = auth.substring(7); token.trim(); }
  }
  if (token.isEmpty()) token = server_.arg("token");
  if (!token.isEmpty() && authService_.sessionValid() &&
      authService_.getCurrentSession().sessionId == token) {
    authService_.logout();
  }
  sendJson(200, "{\"ok\":true}");
}

// --- User management handlers ---

void WebPortal::handleListUsers() {
  if (!requireAuth(UserRole::Admin)) return;
  sendJson(200, "{\"ok\":true,\"users\":" + authService_.listUsersJson() + "}");
}

void WebPortal::handleCreateUser() {
  if (!requireAuth(UserRole::Admin)) return;
  const String username   = server_.arg("username");
  const String password   = server_.arg("password");
  const uint8_t roleVal   = (uint8_t)server_.arg("role").toInt();
  const bool canSetRate   = server_.arg("canSetRate") == "1" || server_.arg("canSetRate") == "true";

  if (username.isEmpty() || password.isEmpty() || roleVal < 1 || roleVal > 3) {
    sendJson(400, "{\"ok\":false,\"message\":\"username, password and role (1-3) required\"}");
    return;
  }

  const bool ok = authService_.createUser(username, password, static_cast<UserRoleLevel>(roleVal), canSetRate);
  sendJson(ok ? 200 : 409,
           ok ? "{\"ok\":true}" : "{\"ok\":false,\"message\":\"user exists or limit reached\"}");
}

void WebPortal::handleUpdateUser() {
  if (!requireAuth(UserRole::Admin)) return;
  const String username = server_.arg("username");
  if (username.isEmpty()) {
    sendJson(400, "{\"ok\":false,\"message\":\"username required\"}");
    return;
  }

  bool ok = true;
  const String newPass    = server_.arg("password");
  const String blockedArg = server_.arg("blocked");
  const String csrArg     = server_.arg("canSetRate");

  if (!newPass.isEmpty())    ok = ok && authService_.updatePassword(username, newPass);
  if (!blockedArg.isEmpty()) ok = ok && authService_.setBlocked(username, blockedArg == "1" || blockedArg == "true");
  if (!csrArg.isEmpty())     ok = ok && authService_.setCanSetRate(username, csrArg == "1" || csrArg == "true");

  sendJson(ok ? 200 : 400, ok ? "{\"ok\":true}" : "{\"ok\":false,\"message\":\"update failed\"}");
}

void WebPortal::handleDeleteUser() {
  if (!requireAuth(UserRole::Admin)) return;
  const String username = server_.arg("username");
  if (username.isEmpty()) {
    sendJson(400, "{\"ok\":false,\"message\":\"username required\"}");
    return;
  }
  const bool ok = authService_.deleteUser(username);
  sendJson(ok ? 200 : 400, ok ? "{\"ok\":true}" : "{\"ok\":false,\"message\":\"cannot delete user\"}");
}

void WebPortal::handleGetSystem() {
  // Manufacturer-only board resource endpoint
  if (!requireAuth(UserRole::Maintenance)) return;

  const size_t freeHeap    = ESP.getFreeHeap();
  const size_t minFreeHeap = ESP.getMinFreeHeap();
  const size_t heapTotal   = ESP.getHeapSize();

  size_t spiffsUsed  = 0;
  size_t spiffsTotal = 0;
  if (SPIFFS.begin(false)) {
    spiffsUsed  = SPIFFS.usedBytes();
    spiffsTotal = SPIFFS.totalBytes();
  }

  const uint32_t uptimeSec = millis() / 1000UL;
  const uint32_t cpuFreq   = ESP.getCpuFreqMHz();
  const int8_t   wifiRssi  = networkManager_.isSTAConnected()
                             ? static_cast<int8_t>(WiFi.RSSI())
                             : 0;

  const String boardTime = rtcService_.initialized()
                         ? rtcService_.getIso8601String()
                         : String("(RTC not set)");
  const SettingsSnapshot cfg = settingsStore_.snapshot();

  String body = "{";
  body += "\"stationId\":"   + jsonStr(cfg.stationId)    + ",";
  body += "\"controllerId\":" + jsonStr(cfg.controllerId) + ",";
  body += "\"siteName\":"    + jsonStr(cfg.siteName)     + ",";
  body += "\"nozzleId\":"    + jsonStr(cfg.nozzleId)     + ",";
  body += "\"freeHeap\":"    + String(freeHeap)    + ",";
  body += "\"minFreeHeap\":" + String(minFreeHeap) + ",";
  body += "\"heapTotal\":"   + String(heapTotal)   + ",";
  body += "\"spiffsUsed\":"  + String(spiffsUsed)  + ",";
  body += "\"spiffsTotal\":" + String(spiffsTotal) + ",";
  body += "\"uptimeSec\":"   + String(uptimeSec)   + ",";
  body += "\"cpuFreqMhz\":"  + String(cpuFreq)     + ",";
  body += "\"wifiRssi\":"    + String(wifiRssi)    + ",";
  body += "\"staIP\":\""     + networkManager_.staIP() + "\",";
  body += "\"apIP\":\""      + networkManager_.apIP()  + "\",";
  body += "\"boardTime\":"   + jsonStr(boardTime)  + ",";
  body += "\"firmware\":\""  + String(BoardConfig::kFirmwareVersion) + "\"";
  body += "}";
  sendJson(200, body);
}

void WebPortal::handleSetSystem() {
  if (!requireAuth(UserRole::Admin)) return;
  const SettingsSnapshot cfg = settingsStore_.snapshot();
  const String stationId = server_.hasArg("stationId") ? server_.arg("stationId") : cfg.stationId;
  const String controllerId = server_.hasArg("controllerId") ? server_.arg("controllerId") : cfg.controllerId;
  const String siteName = server_.hasArg("siteName") ? server_.arg("siteName") : cfg.siteName;
  const String nozzleId = server_.hasArg("nozzleId") ? server_.arg("nozzleId") : cfg.nozzleId;

  if (!settingsStore_.setDeviceIdentity(stationId, controllerId, siteName, nozzleId)) {
    sendJson(400, "{\"ok\":false,\"message\":\"stationId, controllerId and nozzleId are required\"}");
    return;
  }
  sendJson(200, "{\"ok\":true}");
}

void WebPortal::handleGetMqtt() {
  if (!requireAuth(UserRole::Admin)) return;
  const MqttSettingsSnapshot cfg = settingsStore_.mqttSnapshot();
  String body = "{";
  body += "\"enabled\":"   + jsonBool(cfg.enabled)          + ",";
  body += "\"preset\":"    + String(cfg.preset)              + ",";
  body += "\"brokerHost\":" + jsonStr(cfg.brokerHost)        + ",";
  body += "\"brokerPort\":" + String(cfg.brokerPort)         + ",";
  body += "\"topicPrefix\":" + jsonStr(cfg.topicPrefix)      + ",";
  body += "\"clientId\":"  + jsonStr(cfg.clientId)           + ",";
  body += "\"username\":"  + jsonStr(cfg.username)           + ",";
  // Never return password — send a placeholder
  body += "\"hasPassword\":" + jsonBool(!cfg.password.isEmpty());
  body += "}";
  sendJson(200, body);
}

void WebPortal::handleSetMqtt() {
  if (!requireAuth(UserRole::Admin)) return;
  MqttSettingsSnapshot cfg = settingsStore_.mqttSnapshot();

  const String enabledArg = server_.arg("enabled");
  if (!enabledArg.isEmpty())
    cfg.enabled = enabledArg == "1" || enabledArg == "true";

  const String presetArg = server_.arg("preset");
  if (!presetArg.isEmpty()) cfg.preset = static_cast<uint8_t>(presetArg.toInt());

  const String hostArg = server_.arg("brokerHost");
  if (!hostArg.isEmpty()) cfg.brokerHost = hostArg;

  const String portArg = server_.arg("brokerPort");
  if (!portArg.isEmpty()) cfg.brokerPort = static_cast<uint16_t>(portArg.toInt());

  const String prefixArg = server_.arg("topicPrefix");
  if (!prefixArg.isEmpty()) cfg.topicPrefix = prefixArg;

  const String clientIdArg = server_.arg("clientId");
  if (!clientIdArg.isEmpty()) cfg.clientId = clientIdArg;

  const String userArg = server_.arg("username");
  if (!userArg.isEmpty()) cfg.username = userArg;

  const String passArg = server_.arg("password");
  if (!passArg.isEmpty()) cfg.password = passArg;

  const bool ok = settingsStore_.setMqtt(cfg);
  sendJson(ok ? 200 : 500, ok ? "{\"ok\":true}" : "{\"ok\":false,\"message\":\"NVS write failed\"}");
}

void WebPortal::handleMqttTest() {
  if (!requireAuth(UserRole::Admin)) return;
  const MqttSettingsSnapshot cfg = settingsStore_.mqttSnapshot();
  String topic;
  String message;
  const bool ok = mqttService_.publishTest(topic, message);
  String body = "{";
  body += "\"ok\":" + jsonBool(ok) + ",";
  body += "\"compiled\":" + jsonBool(LPG_MQTT_ENABLED != 0) + ",";
  body += "\"enabled\":" + jsonBool(cfg.enabled) + ",";
  body += "\"connected\":" + jsonBool(mqttService_.isConnected()) + ",";
  body += "\"topic\":" + jsonStr(topic) + ",";
  body += "\"message\":" + jsonStr(message);
  body += "}";
  sendJson(ok ? 200 : 400, body);
}

void WebPortal::handleConfigExport() {
  if (!requireAuth(UserRole::Admin)) return;
  const SettingsSnapshot s = settingsStore_.snapshot();
  const MqttSettingsSnapshot m = settingsStore_.mqttSnapshot();
  const ModbusRtuSettings r = settingsStore_.rtuSnapshot();

  String body = "{";
  body += "\"schemaVersion\":1,";
  body += "\"identity\":{";
  body += "\"stationId\":" + jsonStr(s.stationId) + ",";
  body += "\"controllerId\":" + jsonStr(s.controllerId) + ",";
  body += "\"siteName\":" + jsonStr(s.siteName) + ",";
  body += "\"nozzleId\":" + jsonStr(s.nozzleId) + "},";
  body += "\"wifi\":{";
  body += "\"staEnabled\":" + jsonBool(s.staEnabled) + ",";
  body += "\"apEnabled\":" + jsonBool(s.apEnabled) + ",";
  body += "\"autoSwitch\":" + jsonBool(s.wifiAutoSwitch) + ",";
  body += "\"dhcp\":" + jsonBool(s.staDhcp) + ",";
  body += "\"staticIp\":" + jsonStr(s.staStaticIp) + ",";
  body += "\"gateway\":" + jsonStr(s.staGateway) + ",";
  body += "\"subnet\":" + jsonStr(s.staSubnet) + ",";
  body += "\"dns1\":" + jsonStr(s.staDns1) + ",";
  body += "\"dns2\":" + jsonStr(s.staDns2) + ",";
  body += "\"networks\":[";
  for (uint8_t i = 0; i < s.wifiCount; ++i) {
    if (i) body += ",";
    body += "{\"ssid\":" + jsonStr(s.wifiSsid[i]) + ",\"enabled\":" + jsonBool(s.wifiEnabled[i]) + "}";
  }
  body += "]},";
  body += "\"rtu\":{\"enabled\":" + jsonBool(r.enabled) + ",\"slaveAddress\":" + String(r.slaveAddress) +
          ",\"baudRate\":" + String(r.baudRate) + ",\"parity\":" + String(r.parity) +
          ",\"stopBits\":" + String(r.stopBits) + "},";
  body += "\"mqtt\":{\"enabled\":" + jsonBool(m.enabled) + ",\"preset\":" + String(m.preset) +
          ",\"brokerHost\":" + jsonStr(m.brokerHost) + ",\"brokerPort\":" + String(m.brokerPort) +
          ",\"topicPrefix\":" + jsonStr(m.topicPrefix) + ",\"clientId\":" + jsonStr(m.clientId) +
          ",\"username\":" + jsonStr(m.username) + "},";
  body += "\"fill\":{\"ratePerKg\":" + String(s.ratePerKg, 2) +
          ",\"slowFillThreshold\":" + String(s.slowFillThreshold, 3) + "},";
  body += "\"calibration\":{\"valid\":" + jsonBool(weightService_.calibrationValid()) +
          ",\"factor\":" + String(weightService_.calibrationFactor(), 2) + "},";
  body += "\"productionLock\":" + jsonBool(s.productionLocked);
  body += "}";
  sendJson(200, body);
}

void WebPortal::handleConfigImport() {
  if (!requireAuth(UserRole::Admin)) return;
  sendJson(501, "{\"ok\":false,\"message\":\"config import skeleton only; export is available and secrets are excluded\"}");
}

void WebPortal::handleGetCommissioning() {
  if (!requireAuth(UserRole::Maintenance)) return;
  const SettingsSnapshot s = settingsStore_.snapshot();
  const ModbusRtuSettings r = settingsStore_.rtuSnapshot();
  const bool identityConfigured = !s.stationId.isEmpty() && !s.controllerId.isEmpty() && !s.nozzleId.isEmpty();
  const bool networkConfigured = s.wifiCount > 0 || !s.staSsid.isEmpty();
  const bool rtuConfigured = r.enabled && r.slaveAddress >= 1 && r.slaveAddress <= 247 && r.baudRate > 0;
  const bool scaleCalibrated = weightService_.calibrationValid();
  const bool mqttConfigured = settingsStore_.mqttSnapshot().enabled;
  const bool rtcValid = rtcService_.initialized();
  const bool complete = s.commissioningComplete && identityConfigured && networkConfigured &&
                        rtuConfigured && scaleCalibrated && s.inputsVerified && rtcValid;

  String blockers = "[";
  bool first = true;
  auto addBlocker = [&](const char* name) {
    if (!first) blockers += ",";
    first = false;
    blockers += jsonStr(String(name));
  };
  if (!identityConfigured) addBlocker("identityConfigured");
  if (!networkConfigured) addBlocker("networkConfigured");
  if (!rtuConfigured) addBlocker("rtuConfigured");
  if (!scaleCalibrated) addBlocker("scaleCalibrated");
  if (!s.inputsVerified) addBlocker("inputsVerified");
  if (!rtcValid) addBlocker("rtcValid");
  blockers += "]";

  String body = "{";
  body += "\"ok\":true,";
  body += "\"commissioningComplete\":" + jsonBool(complete) + ",";
  body += "\"identityConfigured\":" + jsonBool(identityConfigured) + ",";
  body += "\"networkConfigured\":" + jsonBool(networkConfigured) + ",";
  body += "\"rtuConfigured\":" + jsonBool(rtuConfigured) + ",";
  body += "\"scaleCalibrated\":" + jsonBool(scaleCalibrated) + ",";
  body += "\"inputsVerified\":" + jsonBool(s.inputsVerified) + ",";
  body += "\"mqttConfigured\":" + jsonBool(mqttConfigured) + ",";
  body += "\"rtcValid\":" + jsonBool(rtcValid) + ",";
  body += "\"productionLocked\":" + jsonBool(s.productionLocked) + ",";
  body += "\"blockers\":" + blockers;
  body += "}";
  sendJson(200, body);
}

void WebPortal::handleSetCommissioning() {
  if (!requireAuth(UserRole::Admin)) return;
  const SettingsSnapshot s = settingsStore_.snapshot();
  const bool complete = server_.hasArg("commissioningComplete")
      ? (server_.arg("commissioningComplete") == "1" || server_.arg("commissioningComplete") == "true")
      : s.commissioningComplete;
  const bool inputsVerified = server_.hasArg("inputsVerified")
      ? (server_.arg("inputsVerified") == "1" || server_.arg("inputsVerified") == "true")
      : s.inputsVerified;
  const bool productionLocked = server_.hasArg("productionLocked")
      ? (server_.arg("productionLocked") == "1" || server_.arg("productionLocked") == "true")
      : s.productionLocked;
  const bool ok = settingsStore_.setCommissioningFlags(complete, inputsVerified, productionLocked);
  sendJson(ok ? 200 : 500, ok ? "{\"ok\":true}" : "{\"ok\":false,\"message\":\"NVS write failed\"}");
}

String WebPortal::statusJson() const {
  const StatusSnapshot status = statusStore_.snapshot();

  // Board time from RTC
  const String boardTime = rtcService_.initialized()
                         ? rtcService_.getIso8601String()
                         : String("");

  String body = "{";
  body += "\"state\":\"" + status.stateLabel + "\",";
  body += "\"bootReason\":\"" + status.bootReason + "\",";
  body += "\"boardTime\":" + jsonStr(boardTime) + ",";
  body += "\"staIP\":\"" + networkManager_.staIP() + "\",";
  body += "\"wifiConnected\":" + jsonBool(networkManager_.isSTAConnected()) + ",";
  body += "\"wifiRssi\":" + String(networkManager_.isSTAConnected() ? (int)WiFi.RSSI() : 0) + ",";
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
  body += "\"slowFillThreshold\":" + String(settingsStore_.snapshot().slowFillThreshold, 2) + ",";
  body += "\"nozzleEngaged\":" + jsonBool(status.nozzleEngaged) + ",";
  body += "\"cylinderPresent\":" + jsonBool(status.cylinderPresent) + ",";
  body += "\"emergencyStopOk\":" + jsonBool(status.emergencyStopOk) + ",";
  body += "\"reasonCode\":\"" + status.lastReasonCode + "\",";
  body += "\"calValid\":" + jsonBool(weightService_.calibrationValid()) + ",";
  body += "\"simActive\":" + jsonBool(weightService_.simActive()) + ",";
  body += "\"uptimeMs\":" + String(status.uptimeMs) + ",";
  body += "\"transactionCount\":" + String(transactionLog_.totalCount()) + ",";

  // Composite readiness for UI — mirrors FillController::startFill() checks
  {
    bool ready = true;
    uint16_t readinessMask = 0;
    uint16_t blockerMask = 0;
    String blockers = "[";
    bool first = true;
    auto addBlocker = [&](const char* b, uint16_t bit) {
      if (!first) blockers += ",";
      blockers += "\""; blockers += b; blockers += "\"";
      blockerMask |= bit;
      first = false; ready = false;
    };
    if (status.emergencyStopOk)             readinessMask |= (1U << 0);
    if (status.cylinderPresent)             readinessMask |= (1U << 1);
    if (status.nozzleEngaged)               readinessMask |= (1U << 2);
    if (status.weightStable)                readinessMask |= (1U << 3);
    if (weightService_.calibrationValid())  readinessMask |= (1U << 4);
    if (weightService_.initialized() && !weightService_.readFailed()) readinessMask |= (1U << 5);
    if (status.state == ProcessState::Fault)        addBlocker("active_fault",          1U << 0);
    if (!status.emergencyStopOk)                    addBlocker("estop_active",          1U << 1);
    if (!status.cylinderPresent)                    addBlocker("cylinder_missing",      1U << 2);
    if (!status.nozzleEngaged)                      addBlocker("nozzle_not_engaged",    1U << 3);
    if (!weightService_.initialized())              addBlocker("scale_not_initialized", 1U << 4);
    if (weightService_.readFailed())                addBlocker("scale_read_error",      1U << 5);
    if (!status.weightStable)                       addBlocker("scale_unstable",        1U << 6);
    if (!weightService_.calibrationValid())         addBlocker("scale_not_calibrated",  1U << 7);
    if (weightService_.simActive())                 addBlocker("simulation_active",     1U << 8);
    blockers += "]";
    const bool readinessRequired = status.state == ProcessState::Idle ||
                                   status.state == ProcessState::Ready ||
                                   status.state == ProcessState::Validating ||
                                   status.state == ProcessState::FillingFast ||
                                   status.state == ProcessState::FillingSlow ||
                                   status.state == ProcessState::Settling;
    uint16_t alarmCode = 0;
    if (!status.emergencyStopOk) alarmCode = 1;
    else if (status.lastReasonCode == "nozzle_disengaged" || (readinessRequired && !status.nozzleEngaged)) alarmCode = 2;
    else if (readinessRequired && !status.cylinderPresent) alarmCode = 3;
    else if (weightService_.readFailed()) alarmCode = 4;
    else if (readinessRequired && !status.weightStable && (status.state == ProcessState::Idle || status.state == ProcessState::Ready)) alarmCode = 5;
    else if (readinessRequired && !weightService_.calibrationValid()) alarmCode = 6;
    else if (status.lastReasonCode == "overfill") alarmCode = 7;
    else if (status.lastReasonCode == "fill_timeout") alarmCode = 8;
    else if (status.lastReasonCode == "no_flow") alarmCode = 9;
    else if (status.lastReasonCode == "transaction_log_failed") alarmCode = 10;
    else if (status.lastReasonCode == "operator_stop" || status.lastReasonCode == "serial_stop" || status.lastReasonCode == "modbus_stop") alarmCode = 11;
    else if (status.state == ProcessState::Fault) alarmCode = 12;
    const uint16_t alarmSeverity = (alarmCode == 0) ? 0 : ((alarmCode == 1 || alarmCode == 4 || alarmCode >= 7) ? 3 : 2);
    body += "\"readyToFill\":" + jsonBool(ready) + ",";
    body += "\"blockers\":" + blockers + ",";
    body += "\"readinessMask\":" + String(readinessMask) + ",";
    body += "\"blockerMask\":" + String(blockerMask) + ",";
    body += "\"alarmCode\":" + String(alarmCode) + ",";
    body += "\"alarmSeverity\":" + String(alarmSeverity) + ",";
  }
  body += "\"relays\":[";
  for (uint8_t i = 0; i < 6; ++i) {
    if (i > 0) body += ",";
    body += jsonBool(status.relays[i]);
  }
  body += "],\"inputs\":[";
  for (uint8_t i = 0; i < 6; ++i) {
    if (i > 0) body += ",";
    body += jsonBool(status.inputs[i]);
  }
  body += "]}";
  return body;
}

// GET /api/sd/months?token=X  (Operator+)
void WebPortal::handleGetSdMonths() {
  if (!requireAuth(UserRole::Operator)) return;
  const String body = "{\"ready\":" + jsonBool(sdService_.isReady()) +
                      ",\"months\":" + sdService_.listMonthsJson() +
                      ",\"totalKb\":" + String(sdService_.totalKb()) +
                      ",\"freeKb\":"  + String(sdService_.freeKb()) + "}";
  sendJson(200, body);
}

// GET /api/sd/transactions?month=YYYY-MM&token=X  (Operator+)
void WebPortal::handleGetSdTransactions() {
  if (!requireAuth(UserRole::Operator)) return;
  const String month = server_.arg("month");
  if (month.isEmpty()) {
    sendJson(400, "{\"ok\":false,\"message\":\"month required (YYYY-MM)\"}");
    return;
  }
  const String body = "{\"month\":\"" + month + "\",\"transactions\":" +
                       sdService_.readMonthJson(month) + "}";
  sendJson(200, body);
}

// ── GET /api/stats — transaction statistics (Operator+) ──────────────────────
void WebPortal::handleGetStats() {
  if (!requireAuth(UserRole::Operator)) return;
  const TxnStatsSnapshot s = transactionLog_.computeStats();
  String body = "{";
  body += "\"allCompleted\":"  + String(s.allCompleted)  + ",";
  body += "\"allFailed\":"     + String(s.allFailed)     + ",";
  body += "\"allKg\":"         + String(s.allKg,   3)    + ",";
  body += "\"allAmount\":"     + String(s.allAmount, 2)  + ",";
  body += "\"todayCompleted\":" + String(s.todayCompleted) + ",";
  body += "\"todayFailed\":"   + String(s.todayFailed)   + ",";
  body += "\"todayKg\":"       + String(s.todayKg,   3)  + ",";
  body += "\"todayAmount\":"   + String(s.todayAmount, 2) + ",";
  body += "\"weekCompleted\":"  + String(s.weekCompleted)  + ",";
  body += "\"weekFailed\":"     + String(s.weekFailed)     + ",";
  body += "\"weekKg\":"         + String(s.weekKg,   3)    + ",";
  body += "\"weekAmount\":"     + String(s.weekAmount, 2)  + ",";
  body += "\"monthCompleted\":" + String(s.monthCompleted) + ",";
  body += "\"monthFailed\":"    + String(s.monthFailed)    + ",";
  body += "\"monthKg\":"        + String(s.monthKg,   3)   + ",";
  body += "\"monthAmount\":"    + String(s.monthAmount, 2) + ",";
  body += "\"yearCompleted\":"  + String(s.yearCompleted)  + ",";
  body += "\"yearFailed\":"     + String(s.yearFailed)     + ",";
  body += "\"yearKg\":"         + String(s.yearKg,   3)    + ",";
  body += "\"yearAmount\":"     + String(s.yearAmount, 2);
  body += "}";
  sendJson(200, body);
}

// ── GET /api/time — read RTC clock (any authenticated user) ──────────────────
void WebPortal::handleGetTime() {
  const RtcTime t = rtcService_.getTime();
  String body = "{";
  body += "\"year\":"   + String(t.year)   + ",";
  body += "\"month\":"  + String(t.month)  + ",";
  body += "\"day\":"    + String(t.date)   + ",";
  body += "\"hour\":"   + String(t.hour)   + ",";
  body += "\"minute\":" + String(t.minute) + ",";
  body += "\"second\":" + String(t.second) + ",";
  body += "\"initialized\":" + String(rtcService_.initialized() ? "true" : "false") + ",";
  body += "\"lostPower\":"   + String(rtcService_.lostPower()   ? "true" : "false") + ",";
  body += "\"iso8601\":\"" + rtcService_.getIso8601String() + "\"";
  body += "}";
  sendJson(200, body);
}

// ── POST /api/time — set RTC clock (Admin+) ───────────────────────────────────
void WebPortal::handleSetTime() {
  if (!requireAuth(UserRole::Admin)) return;
  const int year   = server_.arg("year").toInt();
  const int month  = server_.arg("month").toInt();
  const int day    = server_.arg("day").toInt();
  const int hour   = server_.arg("hour").toInt();
  const int minute = server_.arg("minute").toInt();
  const int second = server_.arg("second").toInt();
  if (year < 2020 || month < 1 || month > 12 || day < 1 || day > 31 ||
      hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
    sendJson(400, "{\"message\":\"Invalid date/time values\"}");
    return;
  }
  RtcTime t;
  t.year = static_cast<uint16_t>(year);
  t.month  = static_cast<uint8_t>(month);
  t.date   = static_cast<uint8_t>(day);
  t.hour   = static_cast<uint8_t>(hour);
  t.minute = static_cast<uint8_t>(minute);
  t.second = static_cast<uint8_t>(second);
  t.day    = 1; // day-of-week not critical; RTC keeps it internally
  rtcService_.setTime(t);
  sendJson(200, "{\"message\":\"RTC time set\"}");
}

// ── GET /api/modbus-rtu — read RTU config (Maintenance) ──────────────────────
void WebPortal::handleGetModbusRtu() {
  if (!requireAuth(UserRole::Maintenance)) return;
  const ModbusRtuSettings rtu = settingsStore_.rtuSnapshot();
  String body = "{";
  body += "\"enabled\":"      + String(rtu.enabled ? "true" : "false") + ",";
  body += "\"slaveAddress\":" + String(rtu.slaveAddress) + ",";
  body += "\"baudRate\":"     + String(rtu.baudRate)     + ",";
  body += "\"dataBits\":8,";
  body += "\"parity\":"       + String(rtu.parity)       + ",";
  body += "\"stopBits\":"     + String(rtu.stopBits)     + ",";
  body += "\"rxPin\":"        + String(BoardConfig::kRtuRxPin) + ",";
  body += "\"txPin\":"        + String(BoardConfig::kRtuTxPin) + ",";
  body += "\"dePin\":"        + String(BoardConfig::kRtuDePin) + ",";
  const ResourceSnapshot res = ResourceMonitor::instance().snapshot();
  body += "\"rtuLastUs\":"    + String(res.rtuLastUs)               + ",";
  body += "\"rtuMaxUs\":"     + String(res.rtuMaxUs)                + ",";
  body += "\"rtuAvgUs\":"     + String(res.rtuAvgUs)                + ",";
  body += "\"rtuReqCount\":"  + String(res.modbusRtuRequestCount)   + ",";
  body += "\"rtuErrCount\":"  + String(res.modbusRtuErrorCount)     + ",";
  body += "\"tcpLastUs\":"    + String(res.tcpLastUs)               + ",";
  body += "\"tcpMaxUs\":"     + String(res.tcpMaxUs)                + ",";
  body += "\"tcpAvgUs\":"     + String(res.tcpAvgUs)                + ",";
  body += "\"tcpReqCount\":"  + String(res.modbusTcpRequestCount)   + ",";
  body += "\"tcpErrCount\":"  + String(res.modbusTcpErrorCount);
  body += "}";
  sendJson(200, body);
}

// ── POST /api/modbus-rtu — save RTU config (Maintenance) ─────────────────────
void WebPortal::handleSetModbusRtu() {
  if (!requireAuth(UserRole::Maintenance)) return;
  ModbusRtuSettings rtu = settingsStore_.rtuSnapshot();
  if (server_.hasArg("enabled"))      rtu.enabled      = server_.arg("enabled") == "1";
  if (server_.hasArg("slaveAddress")) rtu.slaveAddress = static_cast<uint8_t>(server_.arg("slaveAddress").toInt());
  if (server_.hasArg("baudRate"))     rtu.baudRate     = static_cast<uint32_t>(server_.arg("baudRate").toInt());
  if (server_.hasArg("dataBits") && server_.arg("dataBits").toInt() != 8) {
    sendJson(400, "{\"message\":\"Invalid RTU data bits; Modbus RTU uses 8 data bits\"}");
    return;
  }
  if (server_.hasArg("parity"))       rtu.parity       = static_cast<uint8_t>(server_.arg("parity").toInt());
  if (server_.hasArg("stopBits"))     rtu.stopBits     = static_cast<uint8_t>(server_.arg("stopBits").toInt());
  if (!settingsStore_.setModbusRtu(rtu)) {
    sendJson(400, "{\"message\":\"Invalid RTU parameters\"}");
    return;
  }
  sendJson(200, "{\"message\":\"RTU settings saved. Changes take effect on next restart.\"}");
}
