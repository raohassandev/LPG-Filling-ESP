#pragma once

#include "ModbusClient.h"
#include "lvgl.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static inline bool ui_label_set_text_if_changed(lv_obj_t* label, const char* text) {
    if (!label || !text) return false;
    const char* old = lv_label_get_text(label);
    if (old && strcmp(old, text) == 0) return false;
    lv_label_set_text(label, text);
    return true;
}

static inline bool ui_label_set_fmt_if_changed(lv_obj_t* label, const char* fmt, ...) {
    if (!label || !fmt) return false;
    char buf[160];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return ui_label_set_text_if_changed(label, buf);
}

static inline bool ui_changed_by(float a, float b, float eps) {
    return fabsf(a - b) >= eps;
}

static inline float sanitizeDisplayKg(float kg) {
    return (kg > -0.005f && kg < 0.005f) ? 0.0f : kg;
}

static inline bool isValidRtcDateTime(const ControllerSnapshot& s) {
    return s.rtcYear >= 2024 && s.rtcYear <= 2099 &&
           s.rtcMonth >= 1 && s.rtcMonth <= 12 &&
           s.rtcDay >= 1 && s.rtcDay <= 31 &&
           s.rtcHour <= 23 && s.rtcMinute <= 59 && s.rtcSecond <= 59;
}

static inline void formatRtcHeader(const ControllerSnapshot& s, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    if (!isValidRtcDateTime(s)) {
        snprintf(out, outSize, "RTC NOT SET");
        return;
    }
    snprintf(out, outSize, "%04u-%02u-%02u %02u:%02u",
             s.rtcYear, s.rtcMonth, s.rtcDay, s.rtcHour, s.rtcMinute);
}

static inline const char* alarmTitle(uint16_t code) {
    switch (code) {
        case 0:  return "No alarm";
        case 1:  return "Emergency stop active";
        case 2:  return "Nozzle not engaged";
        case 3:  return "Cylinder not detected";
        case 4:  return "Scale read error";
        case 5:  return "Scale not stable";
        case 6:  return "Scale not calibrated";
        case 7:  return "Overfill alarm";
        case 8:  return "Fill timeout";
        case 9:  return "No flow detected";
        case 10: return "Transaction log fault";
        case 11: return "Stopped by operator";
        case 12: return "Controller fault active";
        case 13: return "Controller offline";
        default: return "Unknown alarm";
    }
}

static inline const char* alarmHint(uint16_t code) {
    switch (code) {
        case 1:  return "Release E-stop, then press RESET.";
        case 2:  return "Check nozzle switch and wiring.";
        case 3:  return "Place cylinder or check cylinder input.";
        case 4:  return "Check HX711/load cell wiring and power.";
        case 5:  return "Wait for stable weight before starting.";
        case 6:  return "Calibrate scale before filling.";
        case 7:  return "Close valve and check target/scale.";
        case 8:  return "Check valve, gas flow, and timeout setting.";
        case 9:  return "Weight is not increasing. Check gas flow, relay, valve, or simulation.";
        case 10: return "Check SD card/log storage.";
        case 11: return "Press RESET to return to ready state.";
        case 12: return "Inspect controller status and clear the cause.";
        case 13: return "Check RS485 A/B wiring, GND, slave address, and controller power.";
        default: return "Check readiness details, then reset.";
    }
}
