#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "lvgl.h"

inline void display_label_setf(lv_obj_t* label, const char* fmt, ...) {
    if (!label || !fmt) return;
    char buf[160];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    const char* old = lv_label_get_text(label);
    if (old && strcmp(old, buf) == 0) return;
    lv_label_set_text(label, buf);
}
