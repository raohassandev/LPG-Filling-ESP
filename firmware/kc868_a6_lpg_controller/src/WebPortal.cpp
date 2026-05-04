#include "WebPortal.h"

#include <SPIFFS.h>

#include "BoardConfig.h"
#include "ModbusRegisterMap.h"

namespace {
String jsonBool(bool value) { return value ? "true" : "false"; }
}

WebPortal::WebPortal(StatusStore& statusStore, FillController& fillController, WeightService& weightService,
                     SettingsStore& settingsStore, EventLog& eventLog, TransactionLog& transactionLog,
                     RelayBank& relayBank, AuthService& authService, LpgNetworkManager& networkManager)
    : statusStore_(statusStore),
      fillController_(fillController),
      weightService_(weightService),
      settingsStore_(settingsStore),
      eventLog_(eventLog),
      transactionLog_(transactionLog),
      relayBank_(relayBank),
      authService_(authService),
      networkManager_(networkManager) {}

void WebPortal::begin() {
  registerRoutes();
  server_.begin();
  Serial.println(F("[WEB] HTTP server started on port 80"));

  wsServer_.begin();
  wsServer_.onEvent([](uint8_t, WStype_t type, uint8_t*, size_t) {
    if (type == WStype_CONNECTED) Serial.println(F("[WS] Client connected"));
    if (type == WStype_DISCONNECTED) Serial.println(F("[WS] Client disconnected"));
  });
  Serial.println(F("[WS] WebSocket server started on port 81"));
}

void WebPortal::handleClient() {
  server_.handleClient();
  wsServer_.loop();
  broadcastStatus();
}

void WebPortal::broadcastStatus() {
  if (millis() - lastBroadcastMs_ < 200) return;
  lastBroadcastMs_ = millis();
  if (wsServer_.connectedClients() == 0) return;
  String payload = statusJson();
  wsServer_.broadcastTXT(payload);
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
  server_.on("/api/start", HTTP_POST, [this]() { handleStart(); });
  server_.on("/api/stop", HTTP_POST, [this]() { handleStop(); });
  server_.on("/api/reset", HTTP_POST, [this]() { handleReset(); });
  server_.on("/api/sim-weight",  HTTP_POST, [this]() { handleSetSimWeight(); });
  server_.on("/api/sim-clear",   HTTP_POST, [this]() { handleClearSim(); });
  server_.on("/api/sim-inputs",       HTTP_POST, [this]() { handleSetSimInputs(); });
  server_.on("/api/sim-inputs-clear", HTTP_POST, [this]() { handleClearSimInputs(); });
  server_.on("/api/calibrate",   HTTP_POST, [this]() { handleCalibrate(); });
  server_.on("/api/login",   HTTP_POST, [this]() { handleLogin(); });
  server_.on("/api/logout",  HTTP_POST, [this]() { handleLogout(); });
  server_.on("/api/wifi",    HTTP_GET,  [this]() { handleGetWifi(); });
  server_.on("/api/wifi",    HTTP_POST, [this]() { handleSetWifi(); });
  server_.on("/api/network", HTTP_GET,  [this]() { handleGetNetwork(); });
  server_.on("/api/users",         HTTP_GET,    [this]() { handleListUsers(); });
  server_.on("/api/users",         HTTP_POST,   [this]() { handleCreateUser(); });
  server_.on("/api/users/update",  HTTP_POST,   [this]() { handleUpdateUser(); });
  server_.on("/api/users/delete",  HTTP_POST,   [this]() { handleDeleteUser(); });
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

void WebPortal::handleStatus() { sendJson(200, statusJson()); }

void WebPortal::handleWeight() {
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
  const SettingsSnapshot settings = settingsStore_.snapshot();
  String body = "{";
  body += "\"apSsid\":\"" + settings.apSsid + "\",";
  body += "\"slowFillThreshold\":" + String(settings.slowFillThreshold, 2) + ",";
  body += "\"ratePerKg\":" + String(settings.ratePerKg, 2);
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
  const float ratePerKg = server_.arg("ratePerKg").toFloat();
  const bool ok = settingsStore_.setRatePerKg(ratePerKg);
  if (ok) {
    const StatusSnapshot status = statusStore_.snapshot();
    statusStore_.setTargets(status.targetWeightKg, status.targetAmount, ratePerKg);
    eventLog_.append("INFO", "rate_update", "Rate per kg updated from admin UI");
  }
  sendJson(ok ? 200 : 400, ok ? "{\"ok\":true}" : "{\"ok\":false,\"message\":\"invalid rate\"}");
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
  sendJson(200, body);
}

void WebPortal::handleLogs() {
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

void WebPortal::handleCalibrate() {
  if (!requireAuth(UserRole::Maintenance)) return;
  const String factorArg  = server_.arg("factor");
  const String knownKgArg = server_.arg("knownKg");
  const String pointArg   = server_.arg("point");

  if (!factorArg.isEmpty()) {
    // Direct single-point factor — clears two-point mode
    const float factor = factorArg.toFloat();
    if (factor == 0.0f) { sendJson(400, "{\"ok\":false,\"message\":\"factor cannot be zero\"}"); return; }
    weightService_.clearCalPoints();
    weightService_.setCalibrationFactor(factor);
    sendJson(200, "{\"ok\":true,\"mode\":\"single\",\"calFactor\":" + String(factor, 2) + "}");

  } else if (!knownKgArg.isEmpty()) {
    const float knownKg = knownKgArg.toFloat();
    if (knownKg <= 0.0f) { sendJson(400, "{\"ok\":false,\"message\":\"knownKg must be positive\"}"); return; }

    if (!pointArg.isEmpty()) {
      // Two-point calibration: point=1 (low) or point=2 (high)
      const int pt = pointArg.toInt();
      if (pt != 1 && pt != 2) { sendJson(400, "{\"ok\":false,\"message\":\"point must be 1 or 2\"}"); return; }
      weightService_.setCalPoint(static_cast<uint8_t>(pt), knownKg);
      const bool active = weightService_.hasTwoPoints();
      String body = "{\"ok\":true,\"mode\":\"" + String(active ? "two-point" : "two-point-partial") + "\"";
      body += ",\"point\":"    + String(pt);
      body += ",\"rawAbs\":"   + String(pt == 1 ? weightService_.calLowPoint().rawAbs : weightService_.calHighPoint().rawAbs);
      body += ",\"kg\":"       + String(knownKg, 3);
      body += ",\"twoPointActive\":" + String(active ? "true" : "false") + "}";
      sendJson(200, body);

    } else {
      // Legacy single-point: factor = (raw - tare) / knownKg
      const long raw  = weightService_.lastRawValue();
      const long tare = weightService_.tareOffsetRaw();
      const float factor = static_cast<float>(raw - tare) / knownKg;
      if (factor == 0.0f) { sendJson(400, "{\"ok\":false,\"message\":\"raw equals tare — place known weight first\"}"); return; }
      weightService_.clearCalPoints();
      weightService_.setCalibrationFactor(factor);
      sendJson(200, "{\"ok\":true,\"mode\":\"single\",\"calFactor\":" + String(factor, 2)
               + ",\"raw\":" + String(raw) + ",\"tare\":" + String(tare) + "}");
    }
  } else {
    sendJson(400, "{\"ok\":false,\"message\":\"provide factor=, knownKg=, or knownKg= with point=1|2\"}");
  }
}

bool WebPortal::requireAuth(UserRole minRole) {
  const String token = server_.arg("token");
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
  const SettingsSnapshot s = settingsStore_.snapshot();
  String body = "{\"staSsid\":\"" + s.staSsid + "\",\"apSsid\":\"" + s.apSsid + "\"}";
  sendJson(200, body);
}

void WebPortal::handleSetWifi() {
  if (!requireAuth(UserRole::Admin)) return;
  const String ssid = server_.arg("staSsid");
  const String pass = server_.arg("staPassword");
  if (!settingsStore_.setWifi(ssid, pass)) {
    sendJson(400, "{\"ok\":false,\"message\":\"SSID required and password must be 8+ chars\"}");
    return;
  }
  eventLog_.append("INFO", "wifi_update", "WiFi STA credentials updated");
  networkManager_.connectSTA(ssid, pass);
  sendJson(200, "{\"ok\":true,\"message\":\"WiFi credentials saved. Reconnecting...\"}");
}

void WebPortal::handleGetNetwork() {
  String body = "{";
  body += "\"staConnected\":" + String(networkManager_.isSTAConnected() ? "true" : "false") + ",";
  body += "\"staSSID\":\""    + networkManager_.staSSID() + "\",";
  body += "\"staIP\":\""      + networkManager_.staIP()   + "\",";
  body += "\"apSSID\":\""     + networkManager_.apSSID()  + "\",";
  body += "\"apIP\":\""       + networkManager_.apIP()    + "\"";
  body += "}";
  sendJson(200, body);
}

void WebPortal::handleLogout() {
  const String token = server_.arg("token");
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
