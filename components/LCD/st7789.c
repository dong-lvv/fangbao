#include "st7789.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "string.h"

static const char *TAG = "ST7789";
static spi_device_handle_t st7789_spi = NULL;

// DMA缓冲区，用于批量填充颜色
#define ST7789_DMA_BUF_SIZE 4096
static uint8_t *st7789_dma_buf = NULL;

/**
 * @brief 初始化GPIO和SPI总线
 */
void ST7789_SPI_Init(void)
{
    esp_err_t ret;

    // 配置DC、RES、BLK为输出
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << ST7789_PIN_DC) |
                        (1ULL << ST7789_PIN_RES) |
                        (1ULL << ST7789_PIN_BLK),
    };
    gpio_config(&io_conf);

    // 初始状态
    gpio_set_level(ST7789_PIN_RES, 1);
    gpio_set_level(ST7789_PIN_DC, 1);
    gpio_set_level(ST7789_PIN_BLK, 1);  // 背光默认开启

    // SPI总线配置
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = ST7789_PIN_MOSI,
        .sclk_io_num = ST7789_PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ST7789_DMA_BUF_SIZE,
    };

    ret = spi_bus_initialize(ST7789_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    // SPI设备配置
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = ST7789_SPI_FREQ,
        .mode = 0,
        .spics_io_num = ST7789_PIN_CS,
        .queue_size = 7,
        .flags = SPI_DEVICE_NO_DUMMY,
    };

    ret = spi_bus_add_device(ST7789_SPI_HOST, &devcfg, &st7789_spi);
    ESP_ERROR_CHECK(ret);

    // 分配DMA缓冲区
    st7789_dma_buf = (uint8_t *)heap_caps_malloc(ST7789_DMA_BUF_SIZE, MALLOC_CAP_DMA);
    if (st7789_dma_buf == NULL) {
        ESP_LOGE(TAG, "DMA buffer allocation failed!");
    }

    ESP_LOGI(TAG, "SPI initialized, SCK=%d, MOSI=%d, CS=%d, DC=%d, RES=%d",
             ST7789_PIN_SCK, ST7789_PIN_MOSI, ST7789_PIN_CS, ST7789_PIN_DC, ST7789_PIN_RES);
}

/**
 * @brief 写命令
 */
static void ST7789_WriteCommand(uint8_t cmd)
{
    gpio_set_level(ST7789_PIN_DC, 0);

    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_polling_transmit(st7789_spi, &t);
}

/**
 * @brief 写数据（批量）
 */
static void ST7789_WriteData(uint8_t *buff, size_t buff_size)
{
    gpio_set_level(ST7789_PIN_DC, 1);

    while (buff_size > 0) {
        uint32_t chunk_size = buff_size > ST7789_DMA_BUF_SIZE ? ST7789_DMA_BUF_SIZE : buff_size;

        spi_transaction_t t = {
            .length = chunk_size * 8,
            .tx_buffer = buff,
        };
        spi_device_polling_transmit(st7789_spi, &t);

        buff += chunk_size;
        buff_size -= chunk_size;
    }
}

/**
 * @brief 写单字节数据
 */
static void ST7789_WriteSmallData(uint8_t data)
{
    gpio_set_level(ST7789_PIN_DC, 1);

    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
    };
    spi_device_polling_transmit(st7789_spi, &t);
}

/**
 * @brief 设置显示旋转方向
 */
void ST7789_SetRotation(uint8_t m)
{
    ST7789_WriteCommand(ST7789_MADCTL);
    switch (m) {
    case 0:
        ST7789_WriteSmallData(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
        break;
    case 1:
        ST7789_WriteSmallData(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
        break;
    case 2:
        ST7789_WriteSmallData(ST7789_MADCTL_RGB);
        break;
    case 3:
        ST7789_WriteSmallData(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
        break;
    default:
        break;
    }
}

/**
 * @brief 设置地址窗口
 */
static void ST7789_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint16_t x_start = x0 + X_SHIFT, x_end = x1 + X_SHIFT;
    uint16_t y_start = y0 + Y_SHIFT, y_end = y1 + Y_SHIFT;

    ST7789_WriteCommand(ST7789_CASET);
    uint8_t col_data[] = {x_start >> 8, x_start & 0xFF, x_end >> 8, x_end & 0xFF};
    ST7789_WriteData(col_data, sizeof(col_data));

    ST7789_WriteCommand(ST7789_RASET);
    uint8_t row_data[] = {y_start >> 8, y_start & 0xFF, y_end >> 8, y_end & 0xFF};
    ST7789_WriteData(row_data, sizeof(row_data));

    ST7789_WriteCommand(ST7789_RAMWR);
}

/**
 * @brief 初始化ST7789控制器
 */
void ST7789_Init(void)
{
    if (st7789_spi == NULL) {
        ST7789_SPI_Init();
    }

    // 开启背光
    gpio_set_level(ST7789_PIN_BLK, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    // 硬件复位
    gpio_set_level(ST7789_PIN_RES, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(ST7789_PIN_RES, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // 颜色模式
    ST7789_WriteCommand(ST7789_COLMOD);
    ST7789_WriteSmallData(ST7789_COLOR_MODE_16bit);

    // Porch control
    ST7789_WriteCommand(0xB2);
    uint8_t porch_data[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    ST7789_WriteData(porch_data, sizeof(porch_data));

    // 旋转
    ST7789_SetRotation(ST7789_ROTATION);

    // 内部电压生成器设置
    ST7789_WriteCommand(0xB7);
    ST7789_WriteSmallData(0x35);
    ST7789_WriteCommand(0xBB);
    ST7789_WriteSmallData(0x19);
    ST7789_WriteCommand(0xC0);
    ST7789_WriteSmallData(0x2C);
    ST7789_WriteCommand(0xC2);
    ST7789_WriteSmallData(0x01);
    ST7789_WriteCommand(0xC3);
    ST7789_WriteSmallData(0x12);
    ST7789_WriteCommand(0xC4);
    ST7789_WriteSmallData(0x20);
    ST7789_WriteCommand(0xC6);
    ST7789_WriteSmallData(0x0F);
    ST7789_WriteCommand(0xD0);
    ST7789_WriteSmallData(0xA4);
    ST7789_WriteSmallData(0xA1);

    // Gamma设置
    ST7789_WriteCommand(0xE0);
    uint8_t gamma1[] = {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
    ST7789_WriteData(gamma1, sizeof(gamma1));

    ST7789_WriteCommand(0xE1);
    uint8_t gamma2[] = {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23};
    ST7789_WriteData(gamma2, sizeof(gamma2));

    ST7789_WriteCommand(ST7789_INVON);
    ST7789_WriteCommand(ST7789_SLPOUT);
    ST7789_WriteCommand(ST7789_NORON);
    ST7789_WriteCommand(ST7789_DISPON);

    vTaskDelay(pdMS_TO_TICKS(50));
    ST7789_Fill_Color(BLACK);

    ESP_LOGI(TAG, "ST7789 init done, resolution %dx%d", ST7789_WIDTH, ST7789_HEIGHT);
}

/**
 * @brief 全屏填充颜色（DMA优化版）
 */
void ST7789_Fill_Color(uint16_t color)
{
    if (st7789_dma_buf == NULL) return;

    ST7789_SetAddressWindow(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1);

    uint8_t c1 = color >> 8;
    uint8_t c2 = color & 0xFF;

    // 填充DMA缓冲区
    for (int i = 0; i < ST7789_DMA_BUF_SIZE; i += 2) {
        st7789_dma_buf[i] = c1;
        st7789_dma_buf[i + 1] = c2;
    }

    uint32_t total_pixels = ST7789_WIDTH * ST7789_HEIGHT;
    while (total_pixels > 0) {
        uint32_t chunk_pixels = total_pixels > (ST7789_DMA_BUF_SIZE / 2) ? (ST7789_DMA_BUF_SIZE / 2) : total_pixels;
        ST7789_WriteData(st7789_dma_buf, chunk_pixels * 2);
        total_pixels -= chunk_pixels;
    }
}

/**
 * @brief 绘制单个像素
 */
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if ((x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT)) return;

    ST7789_SetAddressWindow(x, y, x, y);
    uint8_t data[] = {color >> 8, color & 0xFF};
    ST7789_WriteData(data, sizeof(data));
}

/**
 * @brief 填充矩形区域（DMA优化版）
 */
void ST7789_Fill(uint16_t xSta, uint16_t ySta, uint16_t xEnd, uint16_t yEnd, uint16_t color)
{
    if ((xEnd >= ST7789_WIDTH) || (yEnd >= ST7789_HEIGHT)) return;
    if (st7789_dma_buf == NULL) return;

    ST7789_SetAddressWindow(xSta, ySta, xEnd, yEnd);

    uint8_t c1 = color >> 8;
    uint8_t c2 = color & 0xFF;

    for (int i = 0; i < ST7789_DMA_BUF_SIZE; i += 2) {
        st7789_dma_buf[i] = c1;
        st7789_dma_buf[i + 1] = c2;
    }

    uint16_t w = xEnd - xSta + 1;
    uint16_t h = yEnd - ySta + 1;
    uint32_t total_pixels = w * h;

    while (total_pixels > 0) {
        uint32_t chunk_pixels = total_pixels > (ST7789_DMA_BUF_SIZE / 2) ? (ST7789_DMA_BUF_SIZE / 2) : total_pixels;
        ST7789_WriteData(st7789_dma_buf, chunk_pixels * 2);
        total_pixels -= chunk_pixels;
    }
}

/**
 * @brief 绘制4px大像素
 */
void ST7789_DrawPixel_4px(uint16_t x, uint16_t y, uint16_t color)
{
    if ((x == 0) || (x >= ST7789_WIDTH - 1) || (y == 0) || (y >= ST7789_HEIGHT - 1)) return;
    ST7789_Fill(x - 1, y - 1, x + 1, y + 1, color);
}

/**
 * @brief 画线（Bresenham算法）
 */
void ST7789_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    uint16_t swap;
    uint16_t steep = ABS(y1 - y0) > ABS(x1 - x0);

    if (steep) {
        swap = x0; x0 = y0; y0 = swap;
        swap = x1; x1 = y1; y1 = swap;
    }

    if (x0 > x1) {
        swap = x0; x0 = x1; x1 = swap;
        swap = y0; y0 = y1; y1 = swap;
    }

    int16_t dx = x1 - x0;
    int16_t dy = ABS(y1 - y0);
    int16_t err = dx / 2;
    int16_t ystep = (y0 < y1) ? 1 : -1;

    for (; x0 <= x1; x0++) {
        if (steep) {
            ST7789_DrawPixel(y0, x0, color);
        } else {
            ST7789_DrawPixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

/**
 * @brief 画矩形边框
 */
void ST7789_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    ST7789_DrawLine(x1, y1, x2, y1, color);
    ST7789_DrawLine(x1, y1, x1, y2, color);
    ST7789_DrawLine(x1, y2, x2, y2, color);
    ST7789_DrawLine(x2, y1, x2, y2, color);
}

/**
 * @brief 画圆
 */
void ST7789_DrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    ST7789_DrawPixel(x0, y0 + r, color);
    ST7789_DrawPixel(x0, y0 - r, color);
    ST7789_DrawPixel(x0 + r, y0, color);
    ST7789_DrawPixel(x0 - r, y0, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        ST7789_DrawPixel(x0 + x, y0 + y, color);
        ST7789_DrawPixel(x0 - x, y0 + y, color);
        ST7789_DrawPixel(x0 + x, y0 - y, color);
        ST7789_DrawPixel(x0 - x, y0 - y, color);
        ST7789_DrawPixel(x0 + y, y0 + x, color);
        ST7789_DrawPixel(x0 - y, y0 + x, color);
        ST7789_DrawPixel(x0 + y, y0 - x, color);
        ST7789_DrawPixel(x0 - y, y0 - x, color);
    }
}

/**
 * @brief 绘制图片
 */
void ST7789_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
    if ((x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT)) return;
    if ((x + w) > ST7789_WIDTH) return;
    if ((y + h) > ST7789_HEIGHT) return;

    ST7789_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    ST7789_WriteData((uint8_t *)data, sizeof(uint16_t) * w * h);
}

/**
 * @brief 反色
 */
void ST7789_InvertColors(uint8_t invert)
{
    ST7789_WriteCommand(invert ? ST7789_INVON : ST7789_INVOFF);
}

/**
 * @brief 写单个字符
 */
void ST7789_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor)
{
    uint32_t i, b, j;
    ST7789_SetAddressWindow(x, y, x + font.width - 1, y + font.height - 1);

    for (i = 0; i < font.height; i++) {
        b = font.data[(ch - 32) * font.height + i];
        for (j = 0; j < font.width; j++) {
            uint8_t data[2];
            if ((b << j) & 0x8000) {
                data[0] = color >> 8;
                data[1] = color & 0xFF;
            } else {
                data[0] = bgcolor >> 8;
                data[1] = bgcolor & 0xFF;
            }
            ST7789_WriteData(data, sizeof(data));
        }
    }
}

/**
 * @brief 写字符串
 */
void ST7789_WriteString(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor)
{
    while (*str) {
        if (x + font.width >= ST7789_WIDTH) {
            x = 0;
            y += font.height;
            if (y + font.height >= ST7789_HEIGHT) {
                break;
            }
            if (*str == ' ') {
                str++;
                continue;
            }
        }
        ST7789_WriteChar(x, y, *str, font, color, bgcolor);
        x += font.width;
        str++;
    }
}

/**
 * @brief 填充矩形
 */
void ST7789_DrawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT) return;
    if ((x + w) > ST7789_WIDTH) w = ST7789_WIDTH - x;
    if ((y + h) > ST7789_HEIGHT) h = ST7789_HEIGHT - y;

    ST7789_Fill(x, y, x + w - 1, y + h - 1, color);
}

/**
 * @brief 画三角形
 */
void ST7789_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color)
{
    ST7789_DrawLine(x1, y1, x2, y2, color);
    ST7789_DrawLine(x2, y2, x3, y3, color);
    ST7789_DrawLine(x3, y3, x1, y1, color);
}

/**
 * @brief 填充三角形
 */
void ST7789_DrawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color)
{
    int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0,
            yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0,
            curpixel = 0;

    deltax = ABS(x2 - x1);
    deltay = ABS(y2 - y1);
    x = x1;
    y = y1;

    if (x2 >= x1) { xinc1 = 1; xinc2 = 1; }
    else { xinc1 = -1; xinc2 = -1; }

    if (y2 >= y1) { yinc1 = 1; yinc2 = 1; }
    else { yinc1 = -1; yinc2 = -1; }

    if (deltax >= deltay) {
        xinc1 = 0; yinc2 = 0; den = deltax;
        num = deltax / 2; numadd = deltay; numpixels = deltax;
    } else {
        xinc2 = 0; yinc1 = 0; den = deltay;
        num = deltay / 2; numadd = deltax; numpixels = deltay;
    }

    for (curpixel = 0; curpixel <= numpixels; curpixel++) {
        ST7789_DrawLine(x, y, x3, y3, color);
        num += numadd;
        if (num >= den) {
            num -= den;
            x += xinc1;
            y += yinc1;
        }
        x += xinc2;
        y += yinc2;
    }
}

/**
 * @brief 填充圆
 */
void ST7789_DrawFilledCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    ST7789_DrawPixel(x0, y0 + r, color);
    ST7789_DrawPixel(x0, y0 - r, color);
    ST7789_DrawPixel(x0 + r, y0, color);
    ST7789_DrawPixel(x0 - r, y0, color);
    ST7789_DrawLine(x0 - r, y0, x0 + r, y0, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        ST7789_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
        ST7789_DrawLine(x0 + x, y0 - y, x0 - x, y0 - y, color);
        ST7789_DrawLine(x0 + y, y0 + x, x0 - y, y0 + x, color);
        ST7789_DrawLine(x0 + y, y0 - x, x0 - y, y0 - x, color);
    }
}

/**
 * @brief 撕裂效应控制
 */
void ST7789_TearEffect(uint8_t tear)
{
    ST7789_WriteCommand(tear ? 0x35 : 0x34);
}

/**
 * @brief 简单测试函数
 */
void ST7789_Test(void)
{
    ST7789_Fill_Color(WHITE);
    vTaskDelay(pdMS_TO_TICKS(500));

    ST7789_WriteString(10, 20, "ESP32-C3 ST7789", Font_11x18, RED, WHITE);
    vTaskDelay(pdMS_TO_TICKS(500));

    ST7789_Fill_Color(CYAN);
    vTaskDelay(pdMS_TO_TICKS(200));
    ST7789_Fill_Color(RED);
    vTaskDelay(pdMS_TO_TICKS(200));
    ST7789_Fill_Color(BLUE);
    vTaskDelay(pdMS_TO_TICKS(200));
    ST7789_Fill_Color(GREEN);
    vTaskDelay(pdMS_TO_TICKS(200));
    ST7789_Fill_Color(YELLOW);
    vTaskDelay(pdMS_TO_TICKS(200));
    ST7789_Fill_Color(WHITE);
    vTaskDelay(pdMS_TO_TICKS(200));

    ST7789_WriteString(10, 10, "Font test", Font_16x26, GBLUE, WHITE);
    ST7789_WriteString(10, 50, "Hello World!", Font_7x10, RED, WHITE);
    ST7789_WriteString(10, 75, "240x280 Display", Font_11x18, YELLOW, WHITE);
    vTaskDelay(pdMS_TO_TICKS(1000));

    ST7789_Fill_Color(RED);
    ST7789_WriteString(10, 10, "Rectangle", Font_11x18, YELLOW, BLACK);
    ST7789_DrawRectangle(30, 40, 100, 110, WHITE);
    vTaskDelay(pdMS_TO_TICKS(500));

    ST7789_Fill_Color(RED);
    ST7789_WriteString(10, 10, "Filled Rect", Font_11x18, YELLOW, BLACK);
    ST7789_DrawFilledRectangle(30, 40, 60, 60, WHITE);
    vTaskDelay(pdMS_TO_TICKS(500));

    ST7789_Fill_Color(RED);
    ST7789_WriteString(10, 10, "Circle", Font_11x18, YELLOW, BLACK);
    ST7789_DrawCircle(80, 100, 30, WHITE);
    vTaskDelay(pdMS_TO_TICKS(500));

    ST7789_Fill_Color(RED);
    ST7789_WriteString(10, 10, "Filled Circle", Font_11x18, YELLOW, BLACK);
    ST7789_DrawFilledCircle(80, 100, 30, WHITE);
    vTaskDelay(pdMS_TO_TICKS(500));

    ST7789_Fill_Color(RED);
    ST7789_WriteString(10, 10, "Triangle", Font_11x18, YELLOW, BLACK);
    ST7789_DrawTriangle(40, 50, 40, 120, 120, 85, WHITE);
    vTaskDelay(pdMS_TO_TICKS(500));

    ST7789_Fill_Color(RED);
    ST7789_WriteString(10, 10, "Filled Tri", Font_11x18, YELLOW, BLACK);
    ST7789_DrawFilledTriangle(40, 50, 40, 120, 120, 85, WHITE);
    vTaskDelay(pdMS_TO_TICKS(500));
}
