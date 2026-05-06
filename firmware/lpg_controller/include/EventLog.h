#pragma once

#include <Arduino.h>

class EventLog {
 public:
  bool begin();
  void append(const char* level, const String& code, const String& message);
  String tail(size_t maxLines = 40) const;

 private:
  static constexpr const char* kLogPath = "/events.log";
};
