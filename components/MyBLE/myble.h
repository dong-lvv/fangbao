#ifndef __MYBLE_H
#define __MYBLE_H

#include <stdint.h>

// 蓝牙初始化函数
int BLE_Init(void);

// 开放给主程序的发送指令函数
void send_cmd_to_bms(uint8_t *cmd, uint16_t len);

#endif /* __MYBLE_H */