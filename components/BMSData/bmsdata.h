// bms_data.h 或在文件顶部
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t soc;
    float remain_cap;
    float total_cap;
    
    float total_vol;
    float volt_diff;
    float temperature;
    uint32_t cycle_count;
    
    bool is_updated; // 数据更新标志位
} bms_data_t;

// 全局变量声明
extern bms_data_t g_bms_data;