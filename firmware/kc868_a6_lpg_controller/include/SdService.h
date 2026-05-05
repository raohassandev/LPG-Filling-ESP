#pragma once

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "TransactionLog.h"

class SdService
{
public:
    explicit SdService() {}

    // Mount SD card on SPI bus using BoardConfig SD pins.
    // Creates /lpg/transactions/ directory tree if absent.
    // Returns true if the card is ready; logs result to Serial.
    bool begin();

    bool isReady() const { return ready_; }

    // Append one CSV row to /lpg/transactions/YYYY-MM.csv.
    // Creates the file with a header row if it does not yet exist.
    // Returns true on success.
    bool appendTransaction(const TransactionRecord& rec);

    // Return a JSON array of month strings present on the card,
    // e.g. ["2025-04","2026-05"], sorted alphabetically.
    // Returns "[]" if not ready or directory is empty.
    String listMonthsJson() const;

    // Return a JSON array of transaction objects from
    // /lpg/transactions/YYYY-MM.csv.
    // Returns "[]" if not ready or file not found.
    String readMonthJson(const String& month) const;

    // SD free space in KB (0 if not ready).
    uint32_t freeKb() const;

    // SD total capacity in KB (0 if not ready).
    uint32_t totalKb() const;

private:
    // Returns "/lpg/transactions/" + month + ".csv"
    static String monthPath(const String& month);

    // Creates the directory at path if it does not exist.
    // Returns true if the directory exists (or was just created).
    bool ensureDir(const char* path);

    bool ready_ = false;
};
