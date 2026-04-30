#pragma once

#include <WebServer.h>

#include "EventLog.h"
#include "FillController.h"
#include "RelayBank.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "TransactionLog.h"
#include "WeightService.h"

class WebPortal {
 public:
  WebPortal(StatusStore& statusStore, FillController& fillController, WeightService& weightService,
            SettingsStore& settingsStore, EventLog& eventLog, TransactionLog& transactionLog, RelayBank& relayBank);

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
  void handleUpdateSettings();
  void handleSetTare();
  void handleZeroNetWeight();
  void handleModbusMap();
  void handleLogs();
  void handleTransactions();
  void handleTransactionsCsv();
  void handleSetRelay();
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
  TransactionLog& transactionLog_;
  RelayBank& relayBank_;
  WebServer server_{80};
};
