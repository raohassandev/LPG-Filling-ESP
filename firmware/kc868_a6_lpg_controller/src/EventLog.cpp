#include "EventLog.h"

#include <SPIFFS.h>

bool EventLog::begin() {
  if (!SPIFFS.exists(kLogPath)) {
    File file = SPIFFS.open(kLogPath, FILE_WRITE);
    if (!file) {
      return false;
    }
    file.close();
  }
  return true;
}

void EventLog::append(const char* level, const String& code, const String& message) {
  File file = SPIFFS.open(kLogPath, FILE_APPEND);
  if (!file) {
    return;
  }

  file.printf("%lu|%s|%s|%s\n", millis(), level, code.c_str(), message.c_str());
  file.close();
}

String EventLog::tail(size_t maxLines) const {
  File file = SPIFFS.open(kLogPath, FILE_READ);
  if (!file) {
    return "";
  }

  String content;
  while (file.available()) {
    content += static_cast<char>(file.read());
  }
  file.close();

  size_t lines = 0;
  for (int i = static_cast<int>(content.length()) - 1; i >= 0; --i) {
    if (content[static_cast<unsigned int>(i)] == '\n') {
      ++lines;
      if (lines > maxLines) {
        return content.substring(static_cast<unsigned int>(i + 1));
      }
    }
  }

  return content;
}
