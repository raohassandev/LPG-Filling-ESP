#pragma once
#include "lvgl.h"

namespace Bsp {
    // Initialise display (RGB panel), touch (GT911), I/O expander (CH422G), LVGL port.
    // Returns false on any hardware failure.
    bool init();

    // Thread-safe LVGL lock — call before any lv_* API, unlock after.
    bool lock(uint32_t timeout_ms = 50);
    void unlock();
}
