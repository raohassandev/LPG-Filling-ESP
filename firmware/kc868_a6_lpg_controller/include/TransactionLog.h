#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#include <time.h>
#include <vector>
#include "BoardConfig.h"
#include "RtcService.h"

// Aggregated statistics snapshot — all-time + per-period
struct TxnStatsSnapshot {
  uint32_t allCompleted = 0;
  uint32_t allFailed    = 0;
  float    allKg        = 0.0f;
  float    allAmount    = 0.0f;

  uint32_t todayCompleted = 0;
  uint32_t todayFailed    = 0;
  float    todayKg        = 0.0f;
  float    todayAmount    = 0.0f;

  uint32_t weekCompleted  = 0;
  uint32_t weekFailed     = 0;
  float    weekKg         = 0.0f;
  float    weekAmount     = 0.0f;

  uint32_t monthCompleted = 0;
  uint32_t monthFailed    = 0;
  float    monthKg        = 0.0f;
  float    monthAmount    = 0.0f;

  uint32_t yearCompleted  = 0;
  uint32_t yearFailed     = 0;
  float    yearKg         = 0.0f;
  float    yearAmount     = 0.0f;
};

// Transaction status
enum class TransactionStatus : uint8_t
{
    Pending,  // Started but not completed
    Complete, // Successfully completed
    Aborted,  // Operator stopped
    Fault,    // Fault condition
};

// Single transaction record
struct TransactionRecord
{
    uint32_t id = 0;
    String transactionId;       // Human-readable ID (e.g., TXN-20260430-0001)
    uint32_t startTimeUnix = 0; // Unix timestamp when started
    uint32_t endTimeUnix = 0;   // Unix timestamp when ended

    float targetWeightKg = 0.0f;
    float finalWeightKg = 0.0f;
    float tareWeightKg = 0.0f;
    float netWeightKg = 0.0f; // finalWeightKg - tareWeightKg

    float ratePerKg = 0.0f;
    float targetAmount = 0.0f;
    float finalAmount = 0.0f;

    TransactionStatus status = TransactionStatus::Pending;
    String reasonCode;
    String faultCode;

    // Metadata
    String operatorUsername;  // logged-in user who started the fill
    String operatorSource;    // "serial" or "api"
    String firmwareVersion;
    String configVersion;
};

class TransactionLog
{
public:
    explicit TransactionLog(RtcService &rtcService) : rtcService_(rtcService) {}

    void begin();

    // Transaction management
    uint32_t startTransaction(float targetKg, float ratePerKg, float targetAmount, float tareWeightKg,
                              const String& source, const String& operatorUsername = "");
    bool completeTransaction(uint32_t id, float finalWeightKg, float netWeightKg);
    bool abortTransaction(uint32_t id, const String &reason);
    bool faultTransaction(uint32_t id, const String &faultCode);

    // Retrieval
    TransactionRecord getTransaction(uint32_t id) const;
    TransactionRecord getLatestTransaction() const;
    std::vector<TransactionRecord> getRecentTransactions(uint8_t count) const;

    // Statistics
    uint32_t totalCount()    const { return nextTransactionId_ - 1; }
    uint32_t completedCount() const { return completedCount_; }
    uint32_t abortedCount()   const { return abortedCount_; }
    uint32_t faultCount()     const { return faultCount_; }
    float    allKgTotal()     const { return allKgTotal_; }
    float    allAmountTotal() const { return allAmountTotal_; }

    // Period-aware aggregation (cached 30 s TTL; call from loop, not ISR)
    TxnStatsSnapshot computeStats() const;
    void invalidateStatsCache() { statsCacheMs_ = 0; }

    // Export
    String exportJson() const;
    String exportJsonForUser(const String& username) const;
    String exportCsv() const;

private:
    uint32_t nextTransactionId_ = 1;
    uint32_t completedCount_    = 0;
    uint32_t abortedCount_      = 0;
    uint32_t faultCount_        = 0;
    float    allKgTotal_        = 0.0f;
    float    allAmountTotal_    = 0.0f;

    mutable TxnStatsSnapshot statsCache_;
    mutable unsigned long    statsCacheMs_ = 0;
    static constexpr unsigned long kStatsCacheMs = 30000UL;

    RtcService &rtcService_;

    static constexpr const char *kTransactionPrefix = "/txn_";

    String generateTransactionId();
    String recordPath(uint32_t id) const;
    bool saveTransaction(const TransactionRecord &record);
    bool loadTransaction(uint32_t id, TransactionRecord &record) const;
    bool parseRecordLine(const String &line, TransactionRecord &record) const;
    uint32_t getCurrentUnixTime();
};
