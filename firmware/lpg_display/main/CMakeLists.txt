file(GLOB_RECURSE SOURCES "*.cpp" "*.c" "screens/*.cpp")

idf_component_register(
    SRCS        ${SOURCES}
    INCLUDE_DIRS "." "screens"
    REQUIRES
        driver
        esp_timer
        freertos
        esp_lcd
        esp_io_expander_ch422g
        esp_lcd_touch_gt911
        esp_lvgl_port
        lvgl
)
