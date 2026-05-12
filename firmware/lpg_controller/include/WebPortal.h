#pragma once

// Define LPG_WEBSOCKET_ENABLED=1 in your build flags to compile WebSocket
// push support (requires the WebSockets library by Markus Sattler / Links2004).
// Without the flag the WebPortal compiles with HTTP-only; the broadcastStatus()
// method becomes a no-op so the rest of the firmware builds with no extra libs.
#ifndef LPG_WEBSOCKET_ENABLED
#define LPG_WEBSOCKET_ENABLED 0
#endif

#include <WebServer.h>
#if LPG_WEBSOCKET_ENABLED
#include <WebSocketsServer.h>
#endif

#include "AuthService.h"
#include "HmiOperationService.h"
#include "MfgPinService.h"
#include "ModbusRegisterCache.h"
#include "NetworkManager.h"
#include "EventLog.h"
#include "FillController.h"
#include "MqttService.h"
#include "RelayBank.h"
#include "RtcService.h"
#include "SdService.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "TransactionLog.h"
#include "WeightService.h"

class WebPortal {
 public:
  WebPortal(StatusStore& statusStore, FillController& fillController, WeightService& weightService,
            SettingsStore& settingsStore, EventLog& eventLog, TransactionLog& transactionLog,
            RelayBank& relayBank, AuthService& authService, LpgNetworkManager& networkManager,
            RtcService& rtcService, SdService& sdService, MqttService& mqttService,
            MfgPinService& mfgPin, ModbusRegisterCache& registerCache);

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
  void handleRelayTestPulse();
  void handleStart();
  void handleStop();
  void handleReset();
  void handleHwTare();
#ifdef LPG_DEV_BUILD
  void handleSetSimWeight();
  void handleClearSim();
  void handleSetSimInputs();
  void handleClearSimInputs();
#endif
  void handleCalibrate();
  void handleLogin();
  void handleLogout();
  void handleGetWifi();
  void handleSetWifi();
  void handleWifiScan();
  void handleGetNetwork();
  void handleGetIoLive();
  void handleListUsers();
  void handleCreateUser();
  void handleUpdateUser();
  void handleDeleteUser();
  void handleGetSystem();
  void handleSetSystem();
  void handleGetMqtt();
  void handleSetMqtt();
  void handleMqttTest();
  void handleConfigExport();
  void handleConfigImport();
  void handleGetCommissioning();
  void handleSetCommissioning();
  void handleGetStats();
  void handleGetTime();
  void handleSetTime();
  void handleGetModbusRtu();
  void handleSetModbusRtu();
  void handleModbusPublic();
  void handleApplyRecommendedRtu();
  void handleGetSdMonths();
  void handleGetSdTransactions();
  // Calibration (Manufacturing PIN required for actions)
  void handleCalibrationTare();
  void handleCalibrationSetPoint();
  void handleCalibrationSave();
  void handleCalibrationCancel();
  // OTA (Manufacturing PIN required)
  void handleOtaStatus();
  void handleOtaUpload();
  // Public Modbus live-data (no auth)
  void handleModbusLive();
  String statusJson() const;
  void broadcastStatus();
  bool requireAuth(UserRole minRole);
  bool requireMfgPin();
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
  RtcService& rtcService_;
  SdService& sdService_;
  MqttService& mqttService_;
  MfgPinService& mfgPin_;
  ModbusRegisterCache& registerCache_;
  bool     otaActive_{false};
  uint32_t otaHandle_{0};
  WebServer server_{80};
#if LPG_WEBSOCKET_ENABLED
  WebSocketsServer wsServer_{81};
  unsigned long lastBroadcastMs_{0};
#endif
};
