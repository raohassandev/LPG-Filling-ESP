#include "SdService.h"

#include <SD.h>
#include <SPI.h>
#include "BoardConfig.h"

// CSV column order matches readMonthJson parser — do not reorder without
// updating both writers and the parser.
static const char* kCsvHeader =
    "id,txnId,startTime,endTime,operator,targetKg,finalKg,netKg,"
    "ratePerKg,finalAmount,status,faultCode\n";

// ── helpers ──────────────────────────────────────────────────────────────────

static uint8_t statusCode(TransactionStatus s) {
  switch (s) {
    case TransactionStatus::Complete: return 1;
    case TransactionStatus::Aborted:  return 2;
    case TransactionStatus::Fault:    return 3;
    default:                          return 0;
  }
}

// Escape double-quotes in a string for JSON string values.
static String jsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '"')       out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else                out += c;
  }
  return out;
}

// Split a CSV line into fields (no quoting — fields must not contain commas).
static std::vector<String> splitCsv(const String& line) {
  std::vector<String> out;
  int start = 0;
  int len = line.length();
  for (int i = 0; i <= len; ++i) {
    if (i == len || line[i] == ',') {
      out.push_back(line.substring(start, i));
      start = i + 1;
    }
  }
  return out;
}

// Convert a UNIX timestamp to "YYYY-MM" for the monthly filename.
static String unixToYearMonth(uint32_t unix) {
  if (unix == 0) return "0000-00";
  // simple calculation without <time.h> — good enough for 2000–2099
  uint32_t days  = unix / 86400;
  uint32_t y = 1970;
  while (true) {
    bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    uint32_t dy = leap ? 366 : 365;
    if (days < dy) break;
    days -= dy; ++y;
  }
  bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
  static const uint8_t dim[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  uint8_t m = 0;
  for (; m < 12; ++m) {
    uint8_t d = (m == 1 && leap) ? 29 : dim[m];
    if (days < d) break;
    days -= d;
  }
  char buf[8];
  snprintf(buf, sizeof(buf), "%04u-%02u", (unsigned)y, (unsigned)(m + 1));
  return String(buf);
}

// ── SdService ────────────────────────────────────────────────────────────────

bool SdService::begin() {
  SPI.begin(BoardConfig::kSdClkPin,
            BoardConfig::kSdMisoPin,
            BoardConfig::kSdMosiPin,
            BoardConfig::kSdCsPin);

  if (!SD.begin(BoardConfig::kSdCsPin)) {
    Serial.println("[SD] mount failed — no card or wiring error");
    ready_ = false;
    return false;
  }

  ensureDir("/lpg");
  ensureDir("/lpg/transactions");

  uint64_t totalMB = SD.totalBytes() / (1024 * 1024);
  uint64_t freeMB  = (SD.totalBytes() - SD.usedBytes()) / (1024 * 1024);
  Serial.printf("[SD] ready — %llu MB total, %llu MB free\n", totalMB, freeMB);

  ready_ = true;
  return true;
}

bool SdService::ensureDir(const char* path) {
  if (SD.exists(path)) return true;
  return SD.mkdir(path);
}

String SdService::monthPath(const String& month) {
  return "/lpg/transactions/" + month + ".csv";
}

bool SdService::appendTransaction(const TransactionRecord& rec) {
  if (!ready_) return false;

  const String month = unixToYearMonth(rec.startTimeUnix);
  const String path  = monthPath(month);
  const bool   newFile = !SD.exists(path);

  File f = SD.open(path, FILE_APPEND);
  if (!f) return false;

  if (newFile) f.print(kCsvHeader);

  // id,txnId,startTime,endTime,operator,targetKg,finalKg,netKg,ratePerKg,finalAmount,status,faultCode
  f.print(rec.id);                        f.print(',');
  f.print(rec.transactionId);             f.print(',');
  f.print(rec.startTimeUnix);             f.print(',');
  f.print(rec.endTimeUnix);               f.print(',');
  f.print(rec.operatorUsername);          f.print(',');
  f.print(rec.targetWeightKg,   3);       f.print(',');
  f.print(rec.finalWeightKg,    3);       f.print(',');
  f.print(rec.netWeightKg,      3);       f.print(',');
  f.print(rec.ratePerKg,        2);       f.print(',');
  f.print(rec.finalAmount,      2);       f.print(',');
  f.print(statusCode(rec.status));        f.print(',');
  f.print(rec.faultCode);                 f.print('\n');
  f.close();
  return true;
}

String SdService::listMonthsJson() const {
  if (!ready_) return "[]";

  File dir = SD.open("/lpg/transactions");
  if (!dir || !dir.isDirectory()) return "[]";

  std::vector<String> months;
  File entry;
  while ((entry = dir.openNextFile())) {
    String name = String(entry.name());
    entry.close();
    if (name.endsWith(".csv")) {
      // entry.name() may return full path on some SD.h versions — strip it
      int slash = name.lastIndexOf('/');
      if (slash >= 0) name = name.substring(slash + 1);
      months.push_back(name.substring(0, name.length() - 4)); // strip .csv
    }
  }
  dir.close();

  // sort ascending
  for (size_t i = 0; i < months.size(); ++i)
    for (size_t j = i + 1; j < months.size(); ++j)
      if (months[j] < months[i]) std::swap(months[i], months[j]);

  String out = "[";
  for (size_t i = 0; i < months.size(); ++i) {
    if (i) out += ',';
    out += '"'; out += months[i]; out += '"';
  }
  out += ']';
  return out;
}

String SdService::readMonthJson(const String& month) const {
  if (!ready_) return "[]";

  const String path = monthPath(month);
  File f = SD.open(path, FILE_READ);
  if (!f) return "[]";

  String out = "[";
  bool first = true;
  bool headerSkipped = false;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) continue;
    if (!headerSkipped) { headerSkipped = true; continue; }

    std::vector<String> cols = splitCsv(line);
    if (cols.size() < 12) continue;

    // Map columns: id,txnId,startTime,endTime,operator,targetKg,finalKg,netKg,
    //              ratePerKg,finalAmount,status,faultCode
    if (!first) out += ',';
    first = false;
    out += "{";
    out += "\"id\":"          + cols[0] + ",";
    out += "\"txnId\":\""     + jsonEscape(cols[1]) + "\",";
    out += "\"startTime\":"   + cols[2] + ",";
    out += "\"endTime\":"     + cols[3] + ",";
    out += "\"operator\":\""  + jsonEscape(cols[4]) + "\",";
    out += "\"targetKg\":"    + cols[5] + ",";
    out += "\"finalKg\":"     + cols[6] + ",";
    out += "\"netKg\":"       + cols[7] + ",";
    out += "\"ratePerKg\":"   + cols[8] + ",";
    out += "\"finalAmount\":" + cols[9] + ",";
    out += "\"status\":"      + cols[10] + ",";
    out += "\"faultCode\":\"" + jsonEscape(cols[11]) + "\"";
    out += "}";
  }
  f.close();
  out += ']';
  return out;
}

uint32_t SdService::freeKb() const {
  if (!ready_) return 0;
  return (uint32_t)((SD.totalBytes() - SD.usedBytes()) / 1024);
}

uint32_t SdService::totalKb() const {
  if (!ready_) return 0;
  return (uint32_t)(SD.totalBytes() / 1024);
}
