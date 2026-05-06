#include <Arduino.h>
#include "DisplayConfig.h"
#include "ModbusClient.h"
#include "UiManager.h"

namespace {
ModbusClient modbusClient;
UiManager    uiManager;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[DISPLAY] LPG Display Controller booting");

  modbusClient.begin();
  uiManager.begin();
}

void loop() {
  modbusClient.poll();
  uiManager.update(modbusClient.snapshot());
  delay(20);
}
