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
  String statusJson() const;

  StatusStore& statusStore_;
  FillController& fillController_;
  WeightService& weightService_;
  SettingsStore& settingsStore_;
  EventLog& eventLog_;
  WebServer server_{80};
};
