#ifndef __CST816D_H
#define __CST816D_H

#include <stdint.h>
#include <stdbool.h>

/* I2C 引脚配置 - 根据实际硬件连线修改 */
#define CST816D_I2C_HOST   0 // I2C_NUM_0
#define CST816D_SDA_PIN    8 // 需根据实际接线修改
#define CST816D_SCL_PIN    9 // 需根据实际接线修改
#define CST816D_I2C_FREQ   400000

/* CST816D I2C 器件地址 */
#define CST816D_I2C_ADDR   0x15

/* 寄存器定义 */
#define CST816D_REG_GESTURE_ID   0x01
#define CST816D_REG_FINGER_NUM   0x02
#define CST816D_REG_XPOS_H       0x03
#define CST816D_REG_XPOS_L       0x04
#define CST816D_REG_YPOS_H       0x05
#define CST816D_REG_YPOS_L       0x06
#define CST816D_REG_CHIP_ID      0xA7

/* 手势 ID 定义 */
#define GESTURE_NONE             0x00
#define GESTURE_SWIPE_UP         0x01
#define GESTURE_SWIPE_DOWN       0x02
#define GESTURE_SWIPE_LEFT       0x03
#define GESTURE_SWIPE_RIGHT      0x04
#define GESTURE_SINGLE_CLICK     0x05
#define GESTURE_DOUBLE_CLICK     0x0B
#define GESTURE_LONG_PRESS       0x0C

/* 坐标结构体 */
typedef struct {
    bool is_pressed;
    uint16_t x;
    uint16_t y;
    uint8_t gesture;
} cst816d_touch_data_t;

/* 函数声明 */
bool cst816d_init(void);
bool cst816d_read_touch(cst816d_touch_data_t *data);

#endif // __CST816D_H
