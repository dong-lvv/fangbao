#include "cst816d.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "CST816D";

/**
 * @brief I2C 写寄存器
 */
static esp_err_t cst816d_write_reg(uint8_t reg, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CST816D_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(CST816D_I2C_HOST, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief I2C 读连续寄存器
 */
static esp_err_t cst816d_read_regs(uint8_t reg, uint8_t *data, size_t len)
{
    if (len == 0) return ESP_OK;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CST816D_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CST816D_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(CST816D_I2C_HOST, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief 初始化 CST816D
 */
bool cst816d_init(void)
{
    esp_err_t ret;

    // 配置 I2C 主机
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CST816D_SDA_PIN,
        .scl_io_num = CST816D_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = CST816D_I2C_FREQ,
    };

    ret = i2c_param_config(CST816D_I2C_HOST, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C config failed");
        return false;
    }

    // 如果已经安装过，就不必重复安装，这里简单处理
    ret = i2c_driver_install(CST816D_I2C_HOST, conf.mode, 0, 0, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "I2C driver install failed");
        return false;
    }

    // 读芯片ID，验证是否连接成功
    uint8_t chip_id = 0;
    ret = cst816d_read_regs(CST816D_REG_CHIP_ID, &chip_id, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read chip ID");
        return false;
    }

    ESP_LOGI(TAG, "CST816D Initialized, Chip ID: 0x%02X", chip_id);

    // 取消休眠或待机模式（如果需要）
    // 写寄存器配置，例如进入自动休眠的时间等
    cst816d_write_reg(0xFA, 0x11); // 使能手势
    cst816d_write_reg(0xED, 0x01); // 开启连续中断模式等

    return true;
}

/**
 * @brief 读取触摸点数据
 */
bool cst816d_read_touch(cst816d_touch_data_t *data)
{
    if (!data) return false;

    uint8_t buf[6];
    // 读取从手势ID(0x01)到Y_L(0x06)的连续6个寄存器
    esp_err_t ret = cst816d_read_regs(CST816D_REG_GESTURE_ID, buf, 6);
    if (ret != ESP_OK) {
        return false;
    }

    data->gesture = buf[0];
    uint8_t finger_num = buf[1] & 0x0F; // 取低4位为按下的点数

    if (finger_num > 0) {
        data->is_pressed = true;
        // X坐标为 X_H 的低4位(最高位常用来标识Event) + X_L
        data->x = ((buf[2] & 0x0F) << 8) | buf[3];
        // Y坐标为 Y_H 的低4位 + Y_L
        data->y = ((buf[4] & 0x0F) << 8) | buf[5];
    } else {
        data->is_pressed = false;
        data->x = 0;
        data->y = 0;
    }

    return true;
}