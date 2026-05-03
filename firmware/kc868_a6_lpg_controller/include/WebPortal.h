#pragma once

#include <WebServer.h>
#include <WebSocketsServer.h>

#include "AuthService.h"
#include "NetworkManager.h"
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
            SettingsStore& settingsStore, EventLog& eventLog, TransactionLog& transactionLog,
            RelayBank& relayBank, AuthService& authService, LpgNetworkManager& networkManager);

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
  void handleHwTare();
  void handleSetSimWeight();
  void handleClearSim();
  void handleCalibrate();
  void handleLogin();
  void handleLogout();
  void handleGetWifi();
  void handleSetWifi();
  void handleGetNetwork();
  String statusJson() const;
  void broadcastStatus();
  bool requireAuth(UserRole minRole);
  void sendCorsHeaders();
  void sendJson(int code, const String& body);

  StatusStore& statusStore_;
  FillController& fillController_;
  WeightService& weightService_;
  SettingsStore& settingsStore_;
  EventLog& eventLog_;
  TransactionLog& transactionLog_;
  RelayBank& relayBank_;
  AuthService& authService_;
  LpgNetworkManager& networkManager_;
  WebServer server_{80};
  WebSocketsServer wsServer_{81};
  unsigned long lastBroadcastMs_{0};
};
