#pragma once

#include <HardwareSerial.h>

#include "BoardConfig.h"
#include "FillController.h"
#include "RtcService.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "TransactionLog.h"

// Modbus RTU server — RS-485 half-duplex via UART2.
// Supports FC01/02/03/05/06/16 with the same register map as ModbusTcpService.
//
// KC868-A6 RS485 wiring:
//   UART2 RX  -> GPIO14  (kRtuRxPin in BoardConfig)
//   UART2 TX  -> GPIO27  (kRtuTxPin)
//   DE/RE pin -> not used; onboard transceiver is auto-direction.

class ModbusRtuService {
 public:
    ModbusRtuService(StatusStore& statusStore, SettingsStore& settingsStore,
                     FillController& fillController, TransactionLog& transactionLog,
                     RtcService& rtcService);

    void begin();
    void handleClient();
    void setMqttConnected(bool v) { mqttConnected_ = v; }

 private:
    void processFrame();
    void sendResponse(const uint8_t* buf, uint16_t len);

    bool  dispatchFC(uint8_t fc, const uint8_t* req, uint16_t reqLen,
                     uint8_t* resp, uint16_t& respLen);
    bool  handleFC01(const uint8_t* req, uint8_t* resp, uint16_t& respLen);
    bool  handleFC02(const uint8_t* req, uint8_t* resp, uint16_t& respLen);
    bool  handleFC03(const uint8_t* req, uint8_t* resp, uint16_t& respLen);
    bool  handleFC05(const uint8_t* req, uint8_t* resp, uint16_t& respLen);
    bool  handleFC06(const uint8_t* req, uint8_t* resp, uint16_t& respLen);
    bool  handleFC16(const uint8_t* req, uint8_t* resp, uint16_t& respLen);

    static uint16_t crc16(const uint8_t* data, uint16_t len);
    static uint16_t readU16BE(const uint8_t* p);

    StatusStore&    statusStore_;
    SettingsStore&  settingsStore_;
    FillController& fillController_;
    TransactionLog& transactionLog_;
    RtcService&     rtcService_;
    HardwareSerial& uart_;
    bool            mqttConnected_{false};

    static constexpr uint16_t kRxBufSize    = 264;
    static constexpr uint32_t kFrameGapMs   = 5;   // inter-frame silence to detect end-of-frame
    static constexpr uint8_t  kBroadcastAddr = 0;

    uint8_t       rxBuf_[kRxBufSize];
    uint16_t      rxLen_{0};
    unsigned long lastByteMs_{0};
    bool          active_{false};
};
