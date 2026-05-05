# SD Card Storage — Implementation Plan

## Why SD Card?

SPIFFS can hold ~2 000–2 500 transactions (≈1.375 MB, ~400 bytes each). After the limit is reached, the oldest records are overwritten. For a single filling station running 100 fills/day that means only ~20–25 days of history.

A 16 GB microSD card at 400 bytes/record holds **40 million records** — more than 1 000 years at 100 fills/day. This makes it the only practical option for year-over-year history without cloud connectivity.

USB is not viable: standard ESP32 has no native USB host; implementing USB-mass-storage (USB MSC) requires external hardware and driver work. SD over SPI is built into the ESP-IDF/Arduino SDK.

---

## Hardware

| Item | Detail |
|------|--------|
| Interface | SPI (4-wire: MOSI, MISO, CLK, CS) |
| Card type | Standard microSD / SDHC up to 32 GB |
| Filesystem | FAT32 (Arduino SD library) |
| KC868-A6 free SPI pins | SPI2: GPIO 13 (MOSI), 12 (MISO), 14 (CLK), CS = GPIO 27 (verify with final board trace) |
| Library | `SD.h` (bundled with Arduino ESP32) |

> Before wiring, confirm that GPIO 13/14/27 are truly free on the KC868-A6 board. The INPUT_EXPANDER (PCF8574) uses I2C, not SPI, so SPI2 is likely available.

---

## File Layout on SD

```
/lpg/
  transactions/
    txn_0000001.csv   ← one file per transaction (simplest, random-access)
    txn_0000002.csv
    …
  daily/
    2025-01.csv       ← optional: daily rollup for fast summary queries
  settings_backup.json
```

**Alternative — monthly append files:**

```
/lpg/transactions/2025-01.csv
/lpg/transactions/2025-02.csv
```

- Faster writes (open → append → close vs. create new file).
- Simpler rotation: delete `2024-01.csv` to free space.
- Recommended for production.

---

## CSV Schema

```
id,startTime,endTime,operator,targetKg,finalKg,ratePerKg,finalAmount,status,faultCode
1,1704067200,1704067260,operator,12.000,11.987,250.00,2996.75,1,
2,1704067320,1704067380,admin,10.000,0.000,250.00,0.00,2,estop
```

Fields match the existing `TransactionRecord` struct exactly — no firmware schema changes needed.

---

## Implementation Steps

### Phase 1 — Hardware bring-up (1 day)

1. Wire microSD module to SPI2 pins.
2. Add `SdService` class (`SdService.h` / `SdService.cpp`) with:
   - `bool begin(uint8_t csPin)` — mounts SD, logs card size/type.
   - `bool isReady() const`
3. Call `sdService.begin(SD_CS_PIN)` in `setup()` after SPIFFS.
4. Test: list root directory via serial command `sdls`.

### Phase 2 — Write path (1–2 days)

1. Add `bool appendTransaction(const TransactionRecord& rec)` to `SdService`.
   - Opens `/lpg/transactions/YYYY-MM.csv` for append (creates if missing).
   - Writes one CSV line.
   - Closes file immediately (ensures data survives sudden power loss).
2. In `loop()`, after `mqttService.publishTransaction(rec)`, also call `sdService.appendTransaction(rec)`.
3. Add serial command `sdcat YYYY-MM` to dump a monthly file.

### Phase 3 — Read path + API (2–3 days)

1. Add `/api/sd/transactions?month=YYYY-MM&token=X` endpoint in `WebPortal`.
   - Streams the CSV file line-by-line as JSON array (chunked transfer).
   - Optional `?page=N&pageSize=100` for pagination.
2. Add `/api/sd/months?token=X` → list of available month files.
3. Update Expo app to fetch SD history when available:
   - New `sdMonths` state, populated from `/api/sd/months`.
   - Period picker gets a "SD Archive" section.

### Phase 4 — Settings backup (0.5 days)

1. On every `saveSettings()` call, also write `settings_backup.json` to SD.
2. Add serial command `sdrestore` that reads `settings_backup.json` and applies it — useful after flash reflash.

### Phase 5 — Space management (0.5 days)

1. On `SdService::begin()`, check free space.
2. If free space < 50 MB (configurable), delete the oldest monthly file.
3. Log the deletion to `EventLog`.

---

## Code Skeleton

```cpp
// include/SdService.h
#pragma once
#include <SD.h>
#include "TransactionLog.h"

class SdService {
public:
  explicit SdService(uint8_t csPin) : csPin_(csPin) {}
  bool begin();
  bool isReady() const { return ready_; }
  bool appendTransaction(const TransactionRecord& rec);
  bool listMonths(String& out);          // JSON array of available months
  bool streamMonth(const String& month, WiFiClient& client);

private:
  uint8_t csPin_;
  bool    ready_ = false;
  String  buildPath(const String& month) const;
};
```

---

## Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| SPI bus conflict with other peripherals | KC868-A6 uses I2C for IO expander — SPI is free; add 100 Ω series resistor on CS line |
| File corruption on power loss mid-write | Open → append single line → close immediately; FAT32 directory entry only updates on close |
| SD card wear | Monthly files, ~3 KB/day at 100 fills; a 4 GB card lasts decades |
| SD absent at boot | `SdService::begin()` returns false; firmware continues without SD, logs warning |
| Large file download over WiFi | Paginate: never send more than 500 records per HTTP response; client requests next page |

---

## Estimated Timeline

| Phase | Effort |
|-------|--------|
| HW bring-up | 1 day |
| Write path | 1–2 days |
| Read API + app | 2–3 days |
| Settings backup | 0.5 days |
| Space management | 0.5 days |
| **Total** | **5–7 days** |
