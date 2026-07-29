#pragma once

#include <WebServer.h>

#include "EventLog.h"
#include "FillController.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "WeightService.h"

class WebPortal {
 public:
  WebPortal(StatusStore& statusStore, FillController& fillController, WeightService& weightService,
            SettingsStore& settingsStore, EventLog& eventLog);

  void begin();
  void handleClient();

 private:
  void registerRoutes();
  void handleRoot();
  void handleHealth();
  void handleVersion();
  void handleStatus();
  void handleWeight();
  void handleSettings();
  void handleLogs();
  void handleStart();
  void handleStop();
  void handleReset();
  void handleSetSimWeight();

  void handleOtaPage();
  void handleOtaStatus();
  void handleOtaUploadComplete();
  void handleOtaUploadChunk();
  void resetOtaState();
  void failOtaUpload(const String& reason);
  bool authorizeOtaRequest();
  bool isOtaStartAllowed() const;

  String statusJson() const;
  String otaStatusJson() const;

  StatusStore& statusStore_;
  FillController& fillController_;
  WeightService& weightService_;
  SettingsStore& settingsStore_;
  EventLog& eventLog_;
  WebServer server_{80};

  bool otaAuthorized_ = false;
  bool otaStarted_ = false;
  bool otaSucceeded_ = false;
  bool otaRestartPending_ = false;
  size_t otaBytesWritten_ = 0;
  String otaError_;
  unsigned long otaRestartAtMs_ = 0;
};
