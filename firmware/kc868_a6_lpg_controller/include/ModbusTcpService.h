#pragma once

#include <WiFi.h>

#include "StatusStore.h"

class ModbusTcpService {
 public:
  explicit ModbusTcpService(StatusStore& statusStore);

  void begin();
  void handleClient();

 private:
  void handleRequest(WiFiClient& client, const uint8_t* request, uint16_t length);
  void sendException(WiFiClient& client, const uint8_t* request, uint8_t functionCode, uint8_t exceptionCode);

  StatusStore& statusStore_;
  WiFiServer server_{502};
};
