#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#include <vector>
#include "BoardConfig.h"
#include "RtcService.h"

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
    String operatorSource; // "serial" or "api"
    String firmwareVersion;
    String configVersion;
};

class TransactionLog
{
public:
    explicit TransactionLog(RtcService &rtcService) : rtcService_(rtcService) {}

    void begin();

    // Transaction management
    uint32_t startTransaction(float targetKg, float ratePerKg, float targetAmount, float tareWeightKg, const String &source);
    bool completeTransaction(uint32_t id, float finalWeightKg, float netWeightKg);
    bool abortTransaction(uint32_t id, const String &reason);
    bool faultTransaction(uint32_t id, const String &faultCode);

    // Retrieval
    TransactionRecord getTransaction(uint32_t id) const;
    TransactionRecord getLatestTransaction() const;
    std::vector<TransactionRecord> getRecentTransactions(uint8_t count) const;

    // Statistics
    uint32_t totalCount() const { return nextTransactionId_ - 1; }
    uint32_t completedCount() const { return completedCount_; }
    uint32_t abortedCount() const { return abortedCount_; }
    uint32_t faultCount() const { return faultCount_; }

    // Export
    String exportJson() const;
    String exportCsv() const;

private:
    uint32_t nextTransactionId_ = 1;
    uint32_t completedCount_ = 0;
    uint32_t abortedCount_ = 0;
    uint32_t faultCount_ = 0;

    RtcService &rtcService_;

    static constexpr const char *kTransactionPrefix = "/txn_";

    String generateTransactionId();
    String recordPath(uint32_t id) const;
    bool saveTransaction(const TransactionRecord &record);
    bool loadTransaction(uint32_t id, TransactionRecord &record) const;
    bool parseRecordLine(const String &line, TransactionRecord &record) const;
    uint32_t getCurrentUnixTime();
};
