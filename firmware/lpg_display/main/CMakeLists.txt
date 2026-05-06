file(GLOB_RECURSE SOURCES "*.cpp" "*.c" "screens/*.cpp")

idf_component_register(
    SRCS        ${SOURCES}
    INCLUDE_DIRS "." "screens"
    REQUIRES
        driver
        esp_timer
        freertos
        esp_lcd
        esp_lcd_touch
        esp_lcd_touch_gt911
        esp_lvgl_port
        lvgl
)
