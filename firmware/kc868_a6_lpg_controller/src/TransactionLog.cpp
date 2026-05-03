#include "TransactionLog.h"
#include "RtcService.h"
#include <cstdlib>
#include <cstring>
#include <time.h>
#include <vector>

void TransactionLog::begin()
{
    nextTransactionId_ = 1;
    completedCount_ = 0;
    abortedCount_ = 0;
    faultCount_ = 0;

    File root = SPIFFS.open("/");
    if (!root)
    {
        Serial.println("[TXN] Transaction log root open failed");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        const String name = file.name();
        file.close();

        const int prefixAt = name.indexOf(kTransactionPrefix);
        if (prefixAt < 0 || !name.endsWith(".txt"))
        {
            file = root.openNextFile();
            continue;
        }

        const uint32_t id = name.substring(prefixAt + strlen(kTransactionPrefix), name.length() - 4).toInt();
        if (id == 0)
        {
            file = root.openNextFile();
            continue;
        }

        if (id >= nextTransactionId_)
        {
            nextTransactionId_ = id + 1;
        }

        TransactionRecord record;
        if (loadTransaction(id, record))
        {
            switch (record.status)
            {
            case TransactionStatus::Complete:
                completedCount_++;
                break;
            case TransactionStatus::Aborted:
                abortedCount_++;
                break;
            case TransactionStatus::Fault:
                faultCount_++;
                break;
            default:
                break;
            }
        }

        file = root.openNextFile();
    }
    root.close();

    Serial.printf("[TXN] Transaction log initialized, next ID: %u\n", nextTransactionId_);
}

uint32_t TransactionLog::startTransaction(float targetKg, float ratePerKg, float targetAmount, float tareWeightKg, const String &source)
{
    TransactionRecord record;
    record.id = nextTransactionId_;
    record.transactionId = generateTransactionId();
    record.startTimeUnix = getCurrentUnixTime();
    record.targetWeightKg = targetKg;
    record.tareWeightKg = tareWeightKg;
    record.ratePerKg = ratePerKg;
    record.targetAmount = targetAmount;
    record.status = TransactionStatus::Pending;
    record.operatorSource = source;
    record.firmwareVersion = BoardConfig::kFirmwareVersion;
    record.configVersion = "1.0.0"; // TODO: Get from settings

    if (saveTransaction(record))
    {
        return nextTransactionId_++;
    }

    return 0;
}

bool TransactionLog::completeTransaction(uint32_t id, float finalWeightKg, float netWeightKg)
{
    TransactionRecord record;
    if (!loadTransaction(id, record))
    {
        return false;
    }

    record.endTimeUnix = getCurrentUnixTime();
    record.finalWeightKg = finalWeightKg;
    record.netWeightKg = netWeightKg < 0.0f ? 0.0f : netWeightKg;
    record.finalAmount = record.netWeightKg * record.ratePerKg;
    record.status = TransactionStatus::Complete;

    if (saveTransaction(record))
    {
        completedCount_++;
        return true;
    }

    return false;
}

bool TransactionLog::abortTransaction(uint32_t id, const String &reason)
{
    TransactionRecord record;
    if (!loadTransaction(id, record))
    {
        return false;
    }

    record.endTimeUnix = getCurrentUnixTime();
    record.status = TransactionStatus::Aborted;
    record.reasonCode = reason;

    if (saveTransaction(record))
    {
        abortedCount_++;
        return true;
    }

    return false;
}

bool TransactionLog::faultTransaction(uint32_t id, const String &faultCode)
{
    TransactionRecord record;
    if (!loadTransaction(id, record))
    {
        return false;
    }

    record.endTimeUnix = getCurrentUnixTime();
    record.status = TransactionStatus::Fault;
    record.faultCode = faultCode;

    if (saveTransaction(record))
    {
        faultCount_++;
        return true;
    }

    return false;
}

TransactionRecord TransactionLog::getTransaction(uint32_t id) const
{
    TransactionRecord record;
    loadTransaction(id, record);
    return record;
}

TransactionRecord TransactionLog::getLatestTransaction() const
{
    if (nextTransactionId_ > 1)
    {
        return getTransaction(nextTransactionId_ - 1);
    }
    return TransactionRecord();
}

std::vector<TransactionRecord> TransactionLog::getRecentTransactions(uint8_t count) const
{
    std::vector<TransactionRecord> records;

    uint32_t startId = (nextTransactionId_ > count) ? (nextTransactionId_ - count) : 1;

    for (uint32_t i = startId; i < nextTransactionId_; i++)
    {
        TransactionRecord record;
        if (loadTransaction(i, record))
        {
            records.push_back(record);
        }
    }

    return records;
}

String TransactionLog::exportJson() const
{
    String json = "{\"transactions\":[";

    for (uint32_t i = 1; i < nextTransactionId_; i++)
    {
        TransactionRecord record;
        if (loadTransaction(i, record))
        {
            if (i > 1)
            {
                json += ",";
            }
            json += "{";
            json += "\"id\":" + String(record.id) + ",";
            json += "\"transactionId\":\"" + record.transactionId + "\",";
            json += "\"startTime\":" + String(record.startTimeUnix) + ",";
            json += "\"endTime\":" + String(record.endTimeUnix) + ",";
            json += "\"targetKg\":" + String(record.targetWeightKg) + ",";
            json += "\"finalKg\":" + String(record.finalWeightKg) + ",";
            json += "\"tareKg\":" + String(record.tareWeightKg) + ",";
            json += "\"netKg\":" + String(record.netWeightKg) + ",";
            json += "\"ratePerKg\":" + String(record.ratePerKg) + ",";
            json += "\"targetAmount\":" + String(record.targetAmount) + ",";
            json += "\"finalAmount\":" + String(record.finalAmount) + ",";
            json += "\"status\":\"" + String(static_cast<uint8_t>(record.status)) + "\",";
            json += "\"reasonCode\":\"" + record.reasonCode + "\",";
            json += "\"faultCode\":\"" + record.faultCode + "\"";
            json += "}";
        }
    }

    json += "]}";
    return json;
}

String TransactionLog::exportCsv() const
{
    String csv = "ID,TransactionID,StartTime,EndTime,TargetKg,FinalKg,TareKg,NetKg,RatePerKg,TargetAmount,FinalAmount,Status,ReasonCode,FaultCode\n";

    for (uint32_t i = 1; i < nextTransactionId_; i++)
    {
        TransactionRecord record;
        if (loadTransaction(i, record))
        {
            csv += String(record.id) + ",";
            csv += record.transactionId + ",";
            csv += String(record.startTimeUnix) + ",";
            csv += String(record.endTimeUnix) + ",";
            csv += String(record.targetWeightKg) + ",";
            csv += String(record.finalWeightKg) + ",";
            csv += String(record.tareWeightKg) + ",";
            csv += String(record.netWeightKg) + ",";
            csv += String(record.ratePerKg) + ",";
            csv += String(record.targetAmount) + ",";
            csv += String(record.finalAmount) + ",";
            csv += String(static_cast<uint8_t>(record.status)) + ",";
            csv += record.reasonCode + ",";
            csv += record.faultCode + "\n";
        }
    }

    return csv;
}

String TransactionLog::generateTransactionId()
{
    // Generate ID like TXN-20260430-0001
    RtcTime time = rtcService_.getTime();
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "TXN-%04u%02u%02u-%04u",
             time.year, time.month, time.date,
             nextTransactionId_);
    return String(buffer);
}

bool TransactionLog::saveTransaction(const TransactionRecord &record)
{
    if (record.id == 0)
    {
        return false;
    }

    File file = SPIFFS.open(recordPath(record.id), FILE_WRITE);
    if (!file)
    {
        return false;
    }

    // Serialize record manually
    char buffer[416] = {0};
    snprintf(buffer, sizeof(buffer),
             "%u|%s|%u|%u|%.3f|%.3f|%.3f|%.3f|%.2f|%.2f|%.2f|%u|%s|%s|%s|%s|%s",
             record.id,
             record.transactionId.c_str(),
             record.startTimeUnix,
             record.endTimeUnix,
             record.targetWeightKg,
             record.finalWeightKg,
             record.tareWeightKg,
             record.netWeightKg,
             record.ratePerKg,
             record.targetAmount,
             record.finalAmount,
             static_cast<uint8_t>(record.status),
             record.reasonCode.c_str(),
             record.faultCode.c_str(),
             record.operatorSource.c_str(),
             record.firmwareVersion.c_str(),
             record.configVersion.c_str());

    const size_t written = file.println(buffer);
    file.close();

    return written > 0;
}

bool TransactionLog::loadTransaction(uint32_t id, TransactionRecord &record) const
{
    if (id == 0)
    {
        return false;
    }

    File file = SPIFFS.open(recordPath(id), FILE_READ);
    if (!file)
    {
        return false;
    }

    const String line = file.readStringUntil('\n');
    file.close();

    return parseRecordLine(line, record);
}

String TransactionLog::recordPath(uint32_t id) const
{
    char path[24];
    snprintf(path, sizeof(path), "%s%06u.txt", kTransactionPrefix, id);
    return String(path);
}

bool TransactionLog::parseRecordLine(const String &line, TransactionRecord &record) const
{
    if (line.isEmpty())
    {
        return false;
    }

    char buffer[416] = {0};
    line.toCharArray(buffer, sizeof(buffer));
    // Strip trailing \r if present (from println's \r\n)
    size_t bufLen = strlen(buffer);
    if (bufLen > 0 && buffer[bufLen - 1] == '\r') buffer[bufLen - 1] = '\0';

    // Split on '|' preserving empty fields (strtok skips them, so use manual split)
    char *tokens[17] = {0};
    uint8_t count = 0;
    char *p = buffer;
    while (count < 17)
    {
        tokens[count++] = p;
        char *delim = strchr(p, '|');
        if (!delim) break;
        *delim = '\0';
        p = delim + 1;
    }

    if (count < 16)
    {
        return false;
    }

    record.id = atoi(tokens[0]);
    record.transactionId = tokens[1];
    record.startTimeUnix = atoi(tokens[2]);
    record.endTimeUnix = atoi(tokens[3]);
    record.targetWeightKg = atof(tokens[4]);
    record.finalWeightKg = atof(tokens[5]);
    if (count >= 17)
    {
        record.tareWeightKg = atof(tokens[6]);
        record.netWeightKg = atof(tokens[7]);
        record.ratePerKg = atof(tokens[8]);
        record.targetAmount = atof(tokens[9]);
        record.finalAmount = atof(tokens[10]);
        record.status = static_cast<TransactionStatus>(atoi(tokens[11]));
        record.reasonCode = tokens[12];
        record.faultCode = tokens[13];
        record.operatorSource = tokens[14];
        record.firmwareVersion = tokens[15];
        record.configVersion = tokens[16];
    }
    else
    {
        record.tareWeightKg = 0.0f;
        record.netWeightKg = atof(tokens[6]);
        record.ratePerKg = atof(tokens[7]);
        record.targetAmount = atof(tokens[8]);
        record.finalAmount = atof(tokens[9]);
        record.status = static_cast<TransactionStatus>(atoi(tokens[10]));
        record.reasonCode = tokens[11];
        record.faultCode = tokens[12];
        record.operatorSource = tokens[13];
        record.firmwareVersion = tokens[14];
        record.configVersion = tokens[15];
    }

    return record.id > 0;
}

uint32_t TransactionLog::getCurrentUnixTime()
{
    // Use RTC if available, otherwise use ESP32 internal time
    if (rtcService_.initialized())
    {
        RtcTime time = rtcService_.getTime();
        // Simple conversion (not handling timezone)
        struct tm t = {0};
        t.tm_year = time.year - 1900;
        t.tm_mon = time.month - 1;
        t.tm_mday = time.date;
        t.tm_hour = time.hour;
        t.tm_min = time.minute;
        t.tm_sec = time.second;
        return mktime(&t);
    }
    return millis() / 1000; // Fallback to uptime
}
