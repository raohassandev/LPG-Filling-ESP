#pragma once

#include <WiFi.h>

#include "FillController.h"
#include "ModbusRegisterCache.h"
#include "RtcService.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "TransactionLog.h"

class ModbusTcpService {
 public:
    ModbusTcpService(StatusStore& statusStore, SettingsStore& settingsStore,
                     FillController& fillController, TransactionLog& transactionLog,
                     RtcService& rtcService, ModbusRegisterCache& registerCache);

    void begin();
    void handleClient();
    void setMqttConnected(bool v) { mqttConnected_ = v; }

 private:
    void dispatchPdu(WiFiClient& client, const uint8_t* mbap, uint8_t fc,
                     const uint8_t* pdu, uint16_t pduLen);

    void handleFC01(WiFiClient& client, const uint8_t* mbap, uint16_t startAddr, uint16_t qty);
    void handleFC02(WiFiClient& client, const uint8_t* mbap, uint16_t startAddr, uint16_t qty);
    void handleFC03(WiFiClient& client, const uint8_t* mbap, uint8_t fc, uint16_t startAddr, uint16_t qty);
    void handleFC05(WiFiClient& client, const uint8_t* mbap, uint16_t coilAddr, uint16_t coilValue);
    void handleFC06(WiFiClient& client, const uint8_t* mbap, uint16_t regAddr,  uint16_t regValue);
    void handleFC16(WiFiClient& client, const uint8_t* mbap, uint16_t startAddr, uint16_t qty,
                    const uint8_t* data);

    void sendException(WiFiClient& client, const uint8_t* mbap, uint8_t fc, uint8_t exCode);

    StatusStore&         statusStore_;
    SettingsStore&       settingsStore_;
    FillController&      fillController_;
    TransactionLog&      transactionLog_;
    RtcService&          rtcService_;
    ModbusRegisterCache& registerCache_;
    WiFiServer           server_{502};
    WiFiClient      activeClient_;
    bool            mqttConnected_{false};

    uint8_t  rxBuf_[260];
    uint16_t rxLen_{0};
};
