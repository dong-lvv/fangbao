/**
 * @file lv_port_disp.c
 * LVGL Display Port - connects LVGL to ST7789 via SPI on ESP32-C3
 *
 * Uses the existing ST7789_DrawImage() function to flush pixel data.
 * Two partial-screen buffers are used for double-buffering.
 */

#include "lv_port_disp.h"
#include "st7789.h"
#include "esp_log.h"
#include "cst816d.h"

static const char *TAG = "lv_port_disp";

/* Draw buffer: 40 lines of 240 pixels each = 9600 pixels = 19200 bytes */
#define LVGL_BUF_LINES  40
static lv_color_t buf_1[ST7789_WIDTH * LVGL_BUF_LINES];

static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t draw_buf;

/**
 * @brief Flush callback - LVGL calls this to push rendered pixels to the display.
 */
/**
 * @brief Flush callback - LVGL calls this to push rendered pixels to the display.
 */
static void disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint16_t w = (uint16_t)(area->x2 - area->x1 + 1);
    uint16_t h = (uint16_t)(area->y2 - area->y1 + 1);
    uint32_t size = w * h;

    /* ==========================================
     * 手动进行高低字节反转，避开编译器的位域 Bug
     * ========================================== */
    uint16_t * buf = (uint16_t *)color_p;
    for(uint32_t i = 0; i < size; i++) {
        buf[i] = (buf[i] >> 8) | (buf[i] << 8);
    }

    /* ST7789_DrawImage sends pixel data to the display */
    ST7789_DrawImage((uint16_t)area->x1, (uint16_t)area->y1, w, h, buf);

    /* Tell LVGL we're done flushing */
    lv_disp_flush_ready(drv);
}

/**
 * @brief Initialize LVGL display driver with ST7789 backend.
 */
void lv_port_disp_init(void)
{
    /* Make sure ST7789 hardware is initialized first */
    ST7789_Init();

    /* Initialize LVGL draw buffer */
    lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, ST7789_WIDTH * LVGL_BUF_LINES);

    /* Initialize display driver */
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = ST7789_WIDTH;
    disp_drv.ver_res = ST7789_HEIGHT;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &draw_buf;

    /* Register the display */
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Display port initialized, buffer: %d lines x %d px = %d bytes",
             LVGL_BUF_LINES, ST7789_WIDTH, (int)sizeof(buf_1));
}

/*------------------
 * Input Device
 *------------------*/

static lv_indev_drv_t indev_drv;

static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    cst816d_touch_data_t touch_data;
    if (cst816d_read_touch(&touch_data)) {
        if (touch_data.is_pressed) {
            data->state = LV_INDEV_STATE_PR;
            // 处理屏幕旋转对触摸坐标的影响
#if ST7789_ROTATION == 0
            data->point.x = touch_data.x;
            data->point.y = touch_data.y;
#elif ST7789_ROTATION == 1
            data->point.x = ST7789_WIDTH - touch_data.y;
            data->point.y = touch_data.x;
#elif ST7789_ROTATION == 2
            data->point.x = ST7789_WIDTH - touch_data.x;
            data->point.y = ST7789_HEIGHT - touch_data.y;
#elif ST7789_ROTATION == 3
            data->point.x = touch_data.y;
            data->point.y = ST7789_HEIGHT - touch_data.x;
#endif
        data->point.x = ST7789_WIDTH - data->point.x;
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
    }
}

void lv_port_indev_init(void)
{
    cst816d_init();

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

