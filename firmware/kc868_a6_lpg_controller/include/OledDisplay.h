#pragma once

#include <Arduino.h>

#include "BoardConfig.h"

class OledDisplay {
 public:
  explicit OledDisplay(const BoardConfig& config) : config_(config) {}

  void begin();
  bool initialized() const { return initialized_; }
  void clear();
  void showLines(const String& line1, const String& line2, const String& line3, const String& line4);

 private:
  const BoardConfig& config_;
  bool initialized_ = false;
  uint8_t buffer_[128 * 8] = {0};

  void command(uint8_t value);
  void flush();
  void drawText(uint8_t row, const String& text);
  void drawChar(uint8_t x, uint8_t page, char c);
  const uint8_t* glyph(char c) const;
};
