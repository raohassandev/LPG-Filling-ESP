#pragma once

#include <Arduino.h>

#include "EventLog.h"
#include "InputExpander.h"
#include "RelayBank.h"
#include "SettingsStore.h"
#include "StatusStore.h"
#include "TransactionLog.h"
#include "WeightService.h"

class FillController {
 public:
  FillController(StatusStore& statusStore, RelayBank& relayBank, InputExpander& inputExpander,
                 WeightService& weightService, SettingsStore& settingsStore, EventLog& eventLog,
                 TransactionLog& transactionLog);

  void begin();
  void tick();

  bool startFill(float targetWeightKg, float ratePerKg, float targetAmount, String& reason);
  bool stopFill(const String& reasonCode);
  bool resetToIdle(String& reason);

 private:
  void syncInputs();
  void syncRelays();
  void transitionTo(ProcessState state, const String& label, const String& reasonCode = "");
  void setFault(const String& reasonCode);
  bool isActiveFillState(ProcessState state) const;

  StatusStore& statusStore_;
  RelayBank& relayBank_;
  InputExpander& inputExpander_;
  WeightService& weightService_;
  SettingsStore& settingsStore_;
  EventLog& eventLog_;
  TransactionLog& transactionLog_;
  uint32_t activeTransactionId_ = 0;
  float fillStartWeightKg_ = 0.0f;
  unsigned long stateStartedMs_ = 0;
};
