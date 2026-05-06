/**
 * lv_conf.h — LVGL 8.x configuration for Waveshare ESP32-S3-Touch-LCD-5
 *
 * Place this file in: Arduino/libraries/lv_conf.h
 * (one level above the lvgl folder, NOT inside it)
 */

#if 1  /* Set to 1 to enable */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH     16    /* 16-bit RGB565 — matches ESP32-S3 LCD_CAM output */
#define LV_COLOR_16_SWAP   0
#define LV_COLOR_SCREEN_TRANSP 0

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_CUSTOM      1    /* Use stdlib malloc/free (we have 8 MB PSRAM) */
#if LV_MEM_CUSTOM == 0
  #define LV_MEM_SIZE     (256 * 1024U)
#else
  #include <stdlib.h>
  #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
  #define LV_MEM_CUSTOM_ALLOC   malloc
  #define LV_MEM_CUSTOM_FREE    free
  #define LV_MEM_CUSTOM_REALLOC realloc
#endif

/*====================
   HAL SETTINGS
 *====================*/
#define LV_DISP_DEF_REFR_PERIOD  16   /* ~60 Hz refresh */
#define LV_INDEV_DEF_READ_PERIOD 16
#define LV_TICK_CUSTOM            1
#if LV_TICK_CUSTOM
  #define LV_TICK_CUSTOM_INCLUDE  <Arduino.h>
  #define LV_TICK_CUSTOM_SYS_TIME_EXPR  (millis())
#endif
#define LV_DPI_DEF 130

/*====================
   FEATURE CONFIGURATION
 *====================*/
#define LV_USE_ANIMATION  1
#define LV_USE_SHADOW     1
#define LV_USE_BLEND_MODES 1
#define LV_USE_OPA_SCALE  1
#define LV_USE_IMG_TRANSFORM 1
#define LV_USE_GROUP      1
#define LV_USE_GPU        0
#define LV_USE_FILESYSTEM 0
#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_USE_ASSERT_STYLE     0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ       0
#define LV_USE_LOG        1
#if LV_USE_LOG
  #define LV_LOG_LEVEL      LV_LOG_LEVEL_WARN
  #define LV_LOG_PRINTF     1
#endif
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0
#define LV_USE_REFR_DEBUG   0
#define LV_SPRINTF_CUSTOM   0
#define LV_USE_USER_DATA    1

/*====================
   LAYOUT
 *====================*/
#define LV_USE_FLEX   1
#define LV_USE_GRID   1

/*====================
   WIDGETS
 *====================*/
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_LABEL_TEXT_SELECTION 1
#define LV_LABEL_LONG_TXT_HINT  1
#define LV_USE_LINE       0
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      0
#define LV_USE_CHART      0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   1
#define LV_USE_LED        0
#define LV_USE_LIST       1
#define LV_USE_MENU       0
#define LV_USE_METER      1
#define LV_USE_MSGBOX     1
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    1
#define LV_USE_TABVIEW    1
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0
#define LV_USE_SPAN       0

/*====================
   THEMES
 *====================*/
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
  #define LV_THEME_DEFAULT_DARK 1   /* Dark mode */
  #define LV_THEME_DEFAULT_GROW 0
  #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO  0

/*====================
   FONTS
 *====================*/
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_UNSCII_8      0
#define LV_FONT_UNSCII_16     0

#define LV_FONT_DEFAULT &lv_font_montserrat_16

#define LV_FONT_FMT_TXT_LARGE 0
#define LV_USE_FONT_SUBPX     0
#define LV_USE_FONT_COMPRESSED 0

/*====================
   TEXT SETTINGS
 *====================*/
#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_COLOR_CMD "#"
#define LV_USE_BIDI       0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*====================
   DRAW/GPU
 *====================*/
#define LV_DRAW_COMPLEX 1
#if LV_DRAW_COMPLEX != 0
  #define LV_SHADOW_CACHE_SIZE 0
  #define LV_CIRCLE_CACHE_COUNT 4
#endif
#define LV_USE_GPU_ARM2D   0
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_SWM341_DMA2D 0
#define LV_USE_GPU_NXP_PXP 0
#define LV_USE_GPU_NXP_VG_LITE 0
#define LV_USE_GPU_SDL 0

/*====================
   IMAGE DECODERS
 *====================*/
#define LV_USE_PNG  0
#define LV_USE_BMP  0
#define LV_USE_JPG  0
#define LV_USE_GIF  0
#define LV_USE_SJPG 0
#define LV_USE_QRCODE 0

/*====================
   DEMOS
 *====================*/
#define LV_USE_DEMO_UNITY_TEST    0
#define LV_USE_DEMO_BENCHMARK     0
#define LV_USE_DEMO_STRESS        0
#define LV_USE_DEMO_MUSIC         0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0

#endif /*LV_CONF_H*/
#endif /*End of "Content enable"*/
