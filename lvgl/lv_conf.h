/**
 * @file lv_conf.h
 * Configuration file for LVGL v8.3.0 - ESP32-C3 + ST7789 240x280
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH      16          /* RGB565 for ST7789 */
#define LV_COLOR_16_SWAP    0           /* 0=normal, 1=swap bytes for SPI 8-bit */
#define LV_COLOR_SCREEN_TRANSP  0
#define LV_COLOR_MIX_ROUND_OFS    0
#define LV_COLOR_CHROMA_KEY     lv_color_hex(0x00ff00)

/*=========================
   MEMORY SETTINGS
 *=========================*/
#define LV_MEM_CUSTOM       0
#if LV_MEM_CUSTOM == 0
    #define LV_MEM_SIZE     (32U * 1024U)   /* 32KB for ESP32-C3 */
    #define LV_MEM_ADR      0
#endif

#define LV_MEM_BUF_MAX_NUM      16
#define LV_MEMCPY_MEMSET_STD    1       /* Use standard memcpy/memset */

/*====================
   HAL SETTINGS
 *====================*/
#define LV_DISP_DEF_REFR_PERIOD     30      /* [ms] */
#define LV_INDEV_DEF_READ_PERIOD    30      /* [ms] */

/* Use custom tick from FreeRTOS */
#define LV_TICK_CUSTOM      1
#define LV_TICK_CUSTOM_INCLUDE   "freertos/FreeRTOS.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (xTaskGetTickCount() * portTICK_PERIOD_MS)

#define LV_DPI_DEF          130

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/

/* Drawing */
#define LV_DRAW_COMPLEX         1
#if LV_DRAW_COMPLEX != 0
    #define LV_SHADOW_CACHE_SIZE    0
    #define LV_CIRCLE_CACHE_SIZE    4
#endif

#define LV_LAYER_SIMPLE_BUF_SIZE            (8 * 1024)
#define LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE   (2 * 1024)

#define LV_IMG_CACHE_DEF_SIZE   0
#define LV_GRADIENT_MAX_STOPS    2
#define LV_GRAD_CACHE_DEF_SIZE   0
#define LV_DITHER_GRADIENT       0
#define LV_DISP_ROT_MAX_BUF      (10 * 1024)

/* GPU: none for ESP32-C3 */
#define LV_USE_GPU_ARM2D        0
#define LV_USE_GPU_STM32_DMA2D  0
#define LV_USE_GPU_SWM341_DMA2D 0
#define LV_USE_GPU_NXP_PXP      0
#define LV_USE_GPU_NXP_VG_LITE  0
#define LV_USE_GPU_SDL          0

/* Log */
#define LV_USE_LOG              1
#if LV_USE_LOG
    #define LV_LOG_LEVEL         LV_LOG_LEVEL_WARN
    #define LV_LOG_PRINTF        1
#endif

/*============
 * OTHERS
 *===========*/

/* Tasks and timers */
#define LV_USE_ASSERT_NULL              1
#define LV_USE_ASSERT_MALLOC            1
#define LV_USE_ASSERT_STYLE             0
#define LV_USE_ASSERT_MEM_INTEGRITY     0
#define LV_USE_ASSERT_OBJ               0
#define LV_USE_ASSERT_STR               0

#define LV_USE_PERF_MONITOR     0
#define LV_USE_MEM_MONITOR       0
#define LV_USE_REFR_DEBUG        0

#define LV_SPRINTF_CUSTOM        0
#define LV_SPRINTF_USE_FLOAT     1

#define LV_USE_USER_DATA         0

/*=============
 * WIDGETS
 *============*/
#define LV_USE_ANIMATION         1
#define LV_USE_GROUP             0

#define LV_USE_ARC               1
#define LV_USE_BAR               1
#define LV_USE_BTN               1
#define LV_USE_BTNMATRIX         1
#define LV_USE_CANVAS            0
#define LV_USE_CHECKBOX          1
#define LV_USE_DROPDOWN          1
#define LV_USE_IMG               1
#define LV_USE_LABEL             1
#define LV_USE_LINE              1
#define LV_USE_ROLLER            0
#define LV_USE_SLIDER            1
#define LV_USE_SWITCH            1
#define LV_USE_TABLE             0
#define LV_USE_TEXTAREA          1

/*=============
 * EXTRA
 *============*/
#define LV_USE_FLEX              1
#define LV_USE_GRID              0
#define LV_USE_FS_STDIO          0
#define LV_USE_FS_POSIX          0
#define LV_USE_FS_WIN32          0
#define LV_USE_FS_FATFS          0

#define LV_USE_THEME_DEFAULT     1
#define LV_USE_THEME_BASIC       1
#define LV_USE_THEME_MONO        0

#define LV_USE_MSG               0
#define LV_USE_SNAPSHOT          0

/*=============
 * FONTS
 *============*/
#define LV_FONT_DEFAULT          &lv_font_montserrat_14
#define LV_FONT_MONTSERRAT_8     0
#define LV_FONT_MONTSERRAT_10    0
#define LV_FONT_MONTSERRAT_12    0
#define LV_FONT_MONTSERRAT_14    1
#define LV_FONT_MONTSERRAT_16    0
#define LV_FONT_MONTSERRAT_18    0
#define LV_FONT_MONTSERRAT_20    1
#define LV_FONT_MONTSERRAT_22    0
#define LV_FONT_MONTSERRAT_24    0
#define LV_FONT_MONTSERRAT_26    0
#define LV_FONT_MONTSERRAT_28    0
#define LV_FONT_MONTSERRAT_30    0
#define LV_FONT_MONTSERRAT_32    1
#define LV_FONT_MONTSERRAT_34    0
#define LV_FONT_MONTSERRAT_36    0
#define LV_FONT_MONTSERRAT_38    0
#define LV_FONT_MONTSERRAT_40    1
#define LV_FONT_MONTSERRAT_42    0
#define LV_FONT_MONTSERRAT_44    0
#define LV_FONT_MONTSERRAT_46    0
#define LV_FONT_MONTSERRAT_48    0
#define LV_FONT_MONTSERRAT_12_SUBPX  0
#define LV_FONT_MONTSERRAT_28_COMPRESSED  0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW  0
#define LV_FONT_SIMSUN_16_CJK    0
#define LV_FONT_UNSCII_8         0
#define LV_FONT_UNSCII_16        0

/*=============
 * FONT USAGE
 *============*/
#define LV_TXT_ENC               LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS       " ,.;:-_)}"
#define LV_TXT_LINE_BREAK_LONG_LEN  0
#define LV_TXT_COLOR_CMD         "#"

/* Alternative font renderer */
#define LV_USE_FONT_COMPRESSED   0
#define LV_USE_FONT_PLACEHOLDER  1

/* Large resolution monitor */
#define LV_USE_LARGE_COORD       0

#endif /* LV_CONF_H */
