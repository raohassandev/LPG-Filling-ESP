#pragma once

#include <cstdarg>
#include <cstdio>

#include "lvgl.h"

inline void display_label_setf(lv_obj_t* label, const char* fmt, ...) {
    if (!label || !fmt) return;
    char buf[160];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    lv_label_set_text(label, buf);
}
