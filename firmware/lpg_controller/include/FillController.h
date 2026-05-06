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

  bool startFill(float targetWeightKg, float ratePerKg, float targetAmount, String& reason,
                 const String& operatorUsername = "");
  bool stopFill(const String& reasonCode);
  bool resetToIdle(String& reason);

  // Simulation — override physical inputs for automated testing
  void setSimInputs(bool cylinderPresent, bool nozzleEngaged);
  void clearSimInputs();

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
  static constexpr unsigned long kMaxFillMs       = 5UL * 60UL * 1000UL; // 5-minute hard timeout
  static constexpr unsigned long kNoFlowWindowMs  = 10UL * 1000UL;       // 10-second no-flow window
  static constexpr float         kNoFlowMinDeltaKg = 0.010f;             // minimum delta to not be "no-flow"
  static constexpr float         kOverfillMarginKg = 0.500f;             // 500 g past target → fault
  static constexpr unsigned long kSettlingMs       = 3UL * 1000UL;       // wait 3 s for scale to stabilise

  uint32_t activeTransactionId_ = 0;
  unsigned long stateStartedMs_ = 0;

  // No-flow detection: snapshot weight at window start, check delta after kNoFlowWindowMs
  unsigned long noFlowWindowStartMs_ = 0;
  float         noFlowWindowStartKg_ = 0.0f;

  bool simInputsActive_    = false;
  bool simCylinderPresent_ = false;
  bool simNozzleEngaged_   = false;
};
