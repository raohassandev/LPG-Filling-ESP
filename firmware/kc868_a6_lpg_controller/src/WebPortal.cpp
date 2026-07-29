#include "WebPortal.h"

#include <SPIFFS.h>
#include <Update.h>

#include "BoardConfig.h"

namespace {
const char* kTrackedHeaders[] = {"X-Manufacturing-PIN"};

String jsonBool(bool value) { return value ? "true" : "false"; }

const char kOtaPage[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>LPG Controller OTA</title>
  <style>
    :root { color-scheme: light; font-family: Arial, sans-serif; }
    body { margin: 0; background: #eef2f5; color: #1d2730; }
    main { width: min(720px, calc(100% - 32px)); margin: 40px auto; }
    .card { background: #fff; border: 1px solid #d8e0e6; border-radius: 14px; padding: 24px; box-shadow: 0 12px 30px rgba(20,40,60,.08); }
    h1 { margin-top: 0; font-size: 1.6rem; }
    .warning { background: #fff6df; border-left: 4px solid #d88a00; padding: 12px; margin: 16px 0; }
    label { display: block; font-weight: 700; margin-top: 16px; }
    input { width: 100%; box-sizing: border-box; margin-top: 7px; padding: 11px; border: 1px solid #b8c4cc; border-radius: 8px; }
    button { margin-top: 20px; padding: 12px 18px; border: 0; border-radius: 8px; background: #163c5a; color: white; font-weight: 700; cursor: pointer; }
    button:disabled { opacity: .55; cursor: not-allowed; }
    progress { width: 100%; height: 18px; margin-top: 18px; }
    pre { white-space: pre-wrap; background: #f5f7f8; padding: 12px; border-radius: 8px; min-height: 40px; }
    .meta { color: #52616b; font-size: .93rem; }
  </style>
</head>
<body>
<main>
  <div class="card">
    <h1>Firmware Update</h1>
    <p class="meta" id="deviceInfo">Reading controller status...</p>
    <div class="warning">
      The controller must be IDLE. Keep power stable, disconnect hazardous actuation where practical,
      and upload only the compiled application <strong>.bin</strong> for this KC868-A6 controller.
    </div>
    <label for="pin">Manufacturing PIN</label>
    <input id="pin" type="password" inputmode="numeric" autocomplete="off" minlength="4" maxlength="8">
    <label for="firmware">Firmware binary</label>
    <input id="firmware" type="file" accept=".bin,application/octet-stream">
    <button id="uploadButton" type="button">Upload firmware</button>
    <progress id="progress" max="100" value="0"></progress>
    <pre id="result">Ready.</pre>
  </div>
</main>
<script>
const button = document.getElementById('uploadButton');
const result = document.getElementById('result');
const progress = document.getElementById('progress');

async function refreshStatus() {
  try {
    const response = await fetch('/api/ota/status', {cache: 'no-store'});
    const data = await response.json();
    document.getElementById('deviceInfo').textContent =
      `${data.device} | firmware ${data.firmwareVersion} | state ${data.state} | OTA space ${data.freeSketchSpace} bytes`;
  } catch (error) {
    document.getElementById('deviceInfo').textContent = 'Unable to read controller status.';
  }
}

button.addEventListener('click', () => {
  const pin = document.getElementById('pin').value.trim();
  const file = document.getElementById('firmware').files[0];
  if (!/^\d{4,8}$/.test(pin)) {
    result.textContent = 'Enter the 4-8 digit manufacturing PIN.';
    return;
  }
  if (!file || !file.name.toLowerCase().endsWith('.bin')) {
    result.textContent = 'Select a valid firmware .bin file.';
    return;
  }

  const form = new FormData();
  form.append('firmware', file, file.name);
  const request = new XMLHttpRequest();
  request.open('POST', '/api/ota/upload');
  request.setRequestHeader('X-Manufacturing-PIN', pin);
  request.upload.onprogress = event => {
    if (event.lengthComputable) progress.value = Math.round((event.loaded / event.total) * 100);
  };
  request.onload = () => {
    button.disabled = false;
    result.textContent = request.responseText || `HTTP ${request.status}`;
    if (request.status === 200) {
      result.textContent += '\nController accepted the firmware and will reboot.';
    }
  };
  request.onerror = () => {
    button.disabled = false;
    result.textContent = 'Upload connection failed.';
  };

  button.disabled = true;
  progress.value = 0;
  result.textContent = 'Uploading. Do not remove power...';
  request.send(form);
});

refreshStatus();
</script>
</body>
</html>
)HTML";
}  // namespace

WebPortal::WebPortal(StatusStore& statusStore, FillController& fillController, WeightService& weightService,
                     SettingsStore& settingsStore, EventLog& eventLog)
    : statusStore_(statusStore),
      fillController_(fillController),
      weightService_(weightService),
      settingsStore_(settingsStore),
      eventLog_(eventLog) {}

void WebPortal::begin() {
  server_.collectHeaders(kTrackedHeaders, sizeof(kTrackedHeaders) / sizeof(kTrackedHeaders[0]));
  registerRoutes();
  server_.begin();
  Serial.println(F("[WEB] HTTP server started on port 80"));
  Serial.println(F("[OTA] Firmware update page available at /ota"));
}

void WebPortal::handleClient() {
  server_.handleClient();

  if (otaRestartPending_ && static_cast<long>(millis() - otaRestartAtMs_) >= 0) {
    Serial.println(F("[OTA] Rebooting into updated firmware"));
    delay(50);
    ESP.restart();
  }
}

void WebPortal::registerRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/health", HTTP_GET, [this]() { handleHealth(); });
  server_.on("/api/version", HTTP_GET, [this]() { handleVersion(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/api/weight", HTTP_GET, [this]() { handleWeight(); });
  server_.on("/api/settings", HTTP_GET, [this]() { handleSettings(); });
  server_.on("/api/logs", HTTP_GET, [this]() { handleLogs(); });
  server_.on("/api/start", HTTP_POST, [this]() { handleStart(); });
  server_.on("/api/stop", HTTP_POST, [this]() { handleStop(); });
  server_.on("/api/reset", HTTP_POST, [this]() { handleReset(); });
  server_.on("/api/sim-weight", HTTP_POST, [this]() { handleSetSimWeight(); });

  server_.on("/ota", HTTP_GET, [this]() { handleOtaPage(); });
  server_.on("/api/ota/status", HTTP_GET, [this]() { handleOtaStatus(); });
  server_.on("/api/ota/upload", HTTP_POST, [this]() { handleOtaUploadComplete(); },
             [this]() { handleOtaUploadChunk(); });
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
  body += "\"manufacturingPinConfigured\":" + jsonBool(!settings.manufacturingPin.isEmpty()) + ",";
  body += "\"slowFillThreshold\":" + String(settings.slowFillThreshold, 2);
  body += "}";
  server_.send(200, "application/json", body);
}

void WebPortal::handleLogs() { server_.send(200, "text/plain", eventLog_.tail()); }

void WebPortal::handleStart() {
  if (otaStarted_ || otaRestartPending_) {
    server_.send(423, "application/json", "{\"ok\":false,\"message\":\"firmware update in progress\"}");
    return;
  }

  const float targetWeightKg = server_.arg("targetWeightKg").toFloat();
  const float ratePerKg = server_.arg("ratePerKg").toFloat();
  const float targetAmount = server_.arg("targetAmount").toFloat();
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
  if (otaStarted_ || otaRestartPending_) {
    server_.send(423, "application/json", "{\"ok\":false,\"message\":\"firmware update in progress\"}");
    return;
  }

  const bool ok = fillController_.stopFill("operator_stop");
  server_.send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true,\"message\":\"stopped\"}"
                                                       : "{\"ok\":false,\"message\":\"no active fill\"}");
}

void WebPortal::handleReset() {
  if (otaStarted_ || otaRestartPending_) {
    server_.send(423, "application/json", "{\"ok\":false,\"message\":\"firmware update in progress\"}");
    return;
  }

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
  if (otaStarted_ || otaRestartPending_) {
    server_.send(423, "application/json", "{\"ok\":false,\"message\":\"firmware update in progress\"}");
    return;
  }

  weightService_.setSimulatedWeightKg(server_.arg("weightKg").toFloat());
  server_.send(200, "application/json", "{\"ok\":true}");
}

void WebPortal::handleOtaPage() { server_.send_P(200, PSTR("text/html"), kOtaPage); }

void WebPortal::handleOtaStatus() { server_.send(200, "application/json", otaStatusJson()); }

void WebPortal::handleOtaUploadComplete() {
  if (!otaAuthorized_) {
    eventLog_.append("WARN", "ota_auth_failed", "Rejected OTA upload with invalid manufacturing PIN");
    server_.send(401, "application/json", "{\"ok\":false,\"message\":\"invalid manufacturing PIN\"}");
    return;
  }

  if (!otaError_.isEmpty()) {
    String body = "{\"ok\":false,\"message\":\"" + otaError_ + "\"}";
    server_.send(400, "application/json", body);
    return;
  }

  if (!otaSucceeded_) {
    server_.send(500, "application/json", "{\"ok\":false,\"message\":\"firmware update did not complete\"}");
    return;
  }

  String body = "{\"ok\":true,\"message\":\"firmware accepted; reboot scheduled\",\"bytesWritten\":";
  body += String(static_cast<unsigned long>(otaBytesWritten_));
  body += "}";
  server_.send(200, "application/json", body);
  otaRestartPending_ = true;
  otaRestartAtMs_ = millis() + 1200UL;
}

void WebPortal::handleOtaUploadChunk() {
  HTTPUpload& upload = server_.upload();

  if (upload.status == UPLOAD_FILE_START) {
    resetOtaState();
    otaAuthorized_ = authorizeOtaRequest();
    if (!otaAuthorized_) {
      otaError_ = "invalid manufacturing PIN";
      Serial.println(F("[OTA] Upload rejected: invalid manufacturing PIN"));
      return;
    }

    if (!isOtaStartAllowed()) {
      otaError_ = "controller must be IDLE before firmware update";
      Serial.println(F("[OTA] Upload rejected: controller is not idle"));
      return;
    }

    String filename = upload.filename;
    filename.toLowerCase();
    if (filename.isEmpty() || !filename.endsWith(".bin")) {
      otaError_ = "firmware filename must end with .bin";
      return;
    }

    if (ESP.getFreeSketchSpace() < 65536U) {
      otaError_ = "insufficient OTA partition space";
      return;
    }

    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      otaError_ = "unable to start OTA writer; error " + String(Update.getError());
      Update.printError(Serial);
      return;
    }

    otaStarted_ = true;
    statusStore_.setState(ProcessState::Maintenance, "MAINTENANCE");
    statusStore_.setReasonCode("ota_update");
    eventLog_.append("INFO", "ota_start", "Authenticated firmware upload started");
    Serial.printf("[OTA] Receiving %s\n", upload.filename.c_str());
    return;
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (!otaStarted_) {
      return;
    }

    if (otaBytesWritten_ == 0 && (upload.currentSize == 0 || upload.buf[0] != 0xE9)) {
      failOtaUpload("invalid ESP32 application image header");
      return;
    }

    const size_t written = Update.write(upload.buf, upload.currentSize);
    if (written != upload.currentSize) {
      failOtaUpload("flash write failed; error " + String(Update.getError()));
      Update.printError(Serial);
      return;
    }

    otaBytesWritten_ += written;
    return;
  }

  if (upload.status == UPLOAD_FILE_END) {
    if (!otaStarted_) {
      return;
    }

    if (otaBytesWritten_ == 0) {
      failOtaUpload("empty firmware image");
      return;
    }

    if (!Update.end(true)) {
      const String reason = "firmware validation failed; error " + String(Update.getError());
      Update.printError(Serial);
      failOtaUpload(reason);
      return;
    }

    otaStarted_ = false;
    otaSucceeded_ = true;
    statusStore_.setReasonCode("ota_ready_reboot");
    eventLog_.append("INFO", "ota_complete", "Firmware verified and staged for reboot");
    Serial.printf("[OTA] Firmware verified, %u bytes written\n", static_cast<unsigned int>(otaBytesWritten_));
    return;
  }

  if (upload.status == UPLOAD_FILE_ABORTED) {
    failOtaUpload("upload aborted by client");
  }
}

void WebPortal::resetOtaState() {
  otaAuthorized_ = false;
  otaStarted_ = false;
  otaSucceeded_ = false;
  otaBytesWritten_ = 0;
  otaError_ = "";
}

void WebPortal::failOtaUpload(const String& reason) {
  if (otaStarted_) {
    Update.abort();
  }

  otaStarted_ = false;
  otaSucceeded_ = false;
  otaError_ = reason;
  statusStore_.setState(ProcessState::Idle, "IDLE");
  statusStore_.setReasonCode("ota_failed");
  eventLog_.append("ERROR", "ota_failed", reason);
  Serial.printf("[OTA] %s\n", reason.c_str());
}

bool WebPortal::authorizeOtaRequest() const {
  const String suppliedPin = server_.header("X-Manufacturing-PIN");
  const String configuredPin = settingsStore_.snapshot().manufacturingPin;
  return !configuredPin.isEmpty() && suppliedPin == configuredPin;
}

bool WebPortal::isOtaStartAllowed() const {
  const StatusSnapshot status = statusStore_.snapshot();
  return status.state == ProcessState::Idle;
}

String WebPortal::statusJson() const {
  const StatusSnapshot status = statusStore_.snapshot();
  String body = "{";
  body += "\"state\":\"" + status.stateLabel + "\",";
  body += "\"bootReason\":\"" + status.bootReason + "\",";
  body += "\"weightKg\":" + String(status.liveWeightKg, 3) + ",";
  body += "\"targetWeightKg\":" + String(status.targetWeightKg, 3) + ",";
  body += "\"targetAmount\":" + String(status.targetAmount, 2) + ",";
  body += "\"ratePerKg\":" + String(status.ratePerKg, 2) + ",";
  body += "\"nozzleEngaged\":" + jsonBool(status.nozzleEngaged) + ",";
  body += "\"cylinderPresent\":" + jsonBool(status.cylinderPresent) + ",";
  body += "\"emergencyStopOk\":" + jsonBool(status.emergencyStopOk) + ",";
  body += "\"reasonCode\":\"" + status.lastReasonCode + "\",";
  body += "\"uptimeMs\":" + String(status.uptimeMs) + ",";
  body += "\"otaBusy\":" + jsonBool(otaStarted_ || otaRestartPending_) + ",";
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

String WebPortal::otaStatusJson() const {
  const StatusSnapshot status = statusStore_.snapshot();
  String body = "{\"device\":\"";
  body += BoardConfig::kDeviceName;
  body += "\",\"firmwareVersion\":\"";
  body += BoardConfig::kFirmwareVersion;
  body += "\",\"state\":\"";
  body += status.stateLabel;
  body += "\",\"allowed\":";
  body += jsonBool(status.state == ProcessState::Idle && !otaStarted_ && !otaRestartPending_);
  body += ",\"busy\":";
  body += jsonBool(otaStarted_ || otaRestartPending_);
  body += ",\"freeSketchSpace\":";
  body += String(static_cast<unsigned long>(ESP.getFreeSketchSpace()));
  body += "}";
  return body;
}
