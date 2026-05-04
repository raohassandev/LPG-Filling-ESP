#pragma once

#include <WiFi.h>

#include "FillController.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "TransactionLog.h"

class ModbusTcpService {
 public:
    ModbusTcpService(StatusStore& statusStore, SettingsStore& settingsStore,
                     FillController& fillController, TransactionLog& transactionLog);

    void begin();
    void handleClient();

 private:
    void handleRequest(WiFiClient& client, const uint8_t* request, uint16_t length);
    void sendException(WiFiClient& client, const uint8_t* request,
                       uint8_t functionCode, uint8_t exceptionCode);

    StatusStore&    statusStore_;
    SettingsStore&  settingsStore_;
    FillController& fillController_;
    TransactionLog& transactionLog_;
    WiFiServer      server_{502};
};
