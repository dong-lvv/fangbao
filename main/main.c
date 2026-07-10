#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "myble.h"
#include "esp_log.h"
#include "ui.h"            // 引入我们刚刚写的 UI 组件

static const char *TAG_MAIN = "MAIN";


void app_main(void)
{
    ESP_LOGI(TAG_MAIN, "Start");
    
    // 1. 初始化 LVGL 核心和底层驱动
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    // 2. 初始化你的应用 UI
    app_ui_init();

    // 3. 初始化蓝牙，开始后台扫描和连接
    BLE_Init();

    // 4. LVGL 专属心跳循环
    while (1)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}