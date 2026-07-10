#include "myble.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <string.h> // 必须包含，用于 memcpy 和 memcmp
#include "bmsdata.h"
static const char *TAG = "MYBLE";

static uint8_t cmd_96[] = {
    0xAA, 0x55, 0x90, 0xEB, 0x96, 0x00, 0xC8, 0x88, 0x7D, 0x97, 
    0xE4, 0xDA, 0xCE, 0x50, 0x45, 0x6B, 0x70, 0x4B, 0x1E, 0xD9
};
static uint8_t cmd_97[] = {
    0xAA, 0x55, 0x90, 0xEB, 0x97, 0x00, 0x2F, 0x66, 0x8F, 0x8E, 
    0x4F, 0xA0, 0x34, 0x03, 0x7C, 0x72, 0x20, 0x46, 0x12, 0x4F
};
// --- 用于数据拼接的缓存 ---
#define RX_BUF_SIZE 300
static uint8_t rx_buf[RX_BUF_SIZE];
static uint16_t rx_len = 0;
extern bms_data_t g_bms_data = {0};
// --- 状态锁 ---
uint8_t data_recving = 0;
uint8_t data_handling = 0;

// ==========================================================
// 1. 定义目标设备的参数 (MAC地址与 UUID)
// ==========================================================
// 目标 MAC 地址：28:D4:1E:1E:EC:35 (注意：在数组中必须反向填写！)
static const uint8_t TARGET_MAC[] = {0x35, 0xEC, 0x1E, 0x1E, 0xD4, 0x28};

// 极空 BMS 的标准 16-bit UUID
static const ble_uuid16_t SVC_UUID = BLE_UUID16_INIT(0xFFE0);
static const ble_uuid16_t CHR_UUID = BLE_UUID16_INIT(0xFFE1);

// 全局变量：分别用于存储物理连接句柄、写入句柄和通知句柄
static uint16_t g_conn_handle = 0;
static uint16_t g_write_handle = 0;
static uint16_t g_notify_handle = 0;

// ==========================================================
// 2. 函数前向声明 
// ==========================================================
static void blecent_scan(void);
static int blecent_gap_event(struct ble_gap_event *event, void *arg);
static int on_svc_discovered(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg);
static int on_chr_discovered(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg);


static void handshake_task(void *pvParameter) {
    ESP_LOGI(TAG, "通道建立完毕，正在发送 0x97...");
    send_cmd_to_bms(cmd_97, sizeof(cmd_97));
    
    // 在这个独立任务里，想怎么延时就怎么延时，绝对不卡底层
    vTaskDelay(200 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "正在发送 0x96...");
    send_cmd_to_bms(cmd_96, sizeof(cmd_96));
    
    ESP_LOGI(TAG, "握手完毕，等待 BMS 回传数据！");
    // 任务执行完后必须删掉自己，释放内存
    vTaskDelete(NULL); 
}
// ==========================================================
// 3. 核心逻辑：发现特征 -> 发现服务
// ==========================================================
static int on_chr_discovered(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg) {
    if (error->status == 0) {
        if (ble_uuid_cmp(&chr->uuid.u, &CHR_UUID.u) == 0) {
            
            if (chr->properties & BLE_GATT_CHR_PROP_NOTIFY) {
                ESP_LOGI(TAG, "找到【通知】特征 FFE1！句柄: %d", chr->val_handle);
                g_notify_handle = chr->val_handle; 
                
                uint8_t notify_en[2] = {0x01, 0x00};
                ble_gattc_write_flat(conn_handle, chr->val_handle + 1, notify_en, sizeof(notify_en), NULL, NULL);
                ESP_LOGI(TAG, "已发出开启 Notify 指令！");
            }
            
            if (chr->properties & (BLE_GATT_CHR_PROP_WRITE | BLE_GATT_CHR_PROP_WRITE_NO_RSP)) {
                ESP_LOGI(TAG, "找到【写入】特征 FFE1！句柄: %d", chr->val_handle);
                g_write_handle = chr->val_handle;
                xTaskCreate(handshake_task, "handshake", 2048, NULL, 5, NULL);
            }
        }
    }
    return 0;
}

static int on_svc_discovered(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg) {
    if (error->status == 0) {
        ESP_LOGI(TAG, "找到服务 FFE0！开始寻找特征 FFE1...");
        ble_gattc_disc_all_chrs(conn_handle, service->start_handle, service->end_handle, on_chr_discovered, NULL);
    }
    return 0;
}

// ==========================================================
// 4. 数据解析提取核心函数
// ==========================================================
static void task_data_handle(uint8_t *buf)
{
    ESP_LOGI(TAG, "================ 电芯数据解析 ==================");

    // 1. 电池总电压 (150~153 字节，小端序，单位 0.001V)
    uint32_t vol_raw = buf[150] | (buf[151] << 8) | (buf[152] << 16) | (buf[153] << 24);
    float total_vol = (float)vol_raw / 1000.0f;
    ESP_LOGI(TAG, "🔋 电池总电压: %.3f V", total_vol);

    // 2. 最大压差 (76~77 字节，小端序，单位 0.001V)
    uint16_t diff_raw = buf[76] | (buf[77] << 8);
    float volt_diff = (float)diff_raw / 1000.0f;
    ESP_LOGI(TAG, "⚖️ 最大压差: %.3f V", volt_diff);

    // 3. 电池温度 (164~165 字节，小端序，单位 0.1摄氏度)
    uint16_t temp_raw = buf[164] | (buf[165] << 8);
    float temperature = (float)temp_raw / 10.0f;
    ESP_LOGI(TAG, "🌡️ 电池温度: %.1f °C", temperature);

    // 4. SOC (173 字节，十进制就是百分比)
    uint8_t soc = buf[173];
    ESP_LOGI(TAG, "🔋 剩余电量 (SOC): %d %%", soc);

    // 5. 剩余容量 (174~177 字节，小端序，单位 0.001AH)
    uint32_t rem_cap_raw = buf[174] | (buf[175] << 8) | (buf[176] << 16) | (buf[177] << 24);
    float remain_cap = (float)rem_cap_raw / 1000.0f;
    ESP_LOGI(TAG, "⚡ 剩余容量: %.3f AH", remain_cap);

    // 6. 总容量 (178~181 字节，小端序，单位 0.001AH)
    uint32_t total_cap_raw = buf[178] | (buf[179] << 8) | (buf[180] << 16) | (buf[181] << 24);
    float total_cap = (float)total_cap_raw / 1000.0f;
    ESP_LOGI(TAG, "🔋 电池总容量: %.3f AH", total_cap);

    // 7. 循环计数 (182~185 字节，小端序)
    uint32_t cycle_count = buf[182] | (buf[183] << 8) | (buf[184] << 16) | (buf[185] << 24);
    ESP_LOGI(TAG, "🔄 循环次数: %lu 次", (unsigned long)cycle_count);

    ESP_LOGI(TAG, "================================================");
// 写入全局结构体
    g_bms_data.total_vol = (float)(buf[150] | (buf[151] << 8) | (buf[152] << 16) | (buf[153] << 24)) / 1000.0f;
    g_bms_data.volt_diff = (float)(buf[76] | (buf[77] << 8)) / 1000.0f;
    g_bms_data.temperature = (float)(buf[164] | (buf[165] << 8)) / 10.0f;
    g_bms_data.soc = buf[173];
    g_bms_data.remain_cap = (float)(buf[174] | (buf[175] << 8) | (buf[176] << 16) | (buf[177] << 24)) / 1000.0f;
    g_bms_data.total_cap = (float)(buf[178] | (buf[179] << 8) | (buf[180] << 16) | (buf[181] << 24)) / 1000.0f;
    g_bms_data.cycle_count = buf[182] | (buf[183] << 8) | (buf[184] << 16) | (buf[185] << 24);
    // 触发 UI 刷新标志
    g_bms_data.is_updated = true;
    // 处理完毕，释放锁
    data_handling = 0;
}

// ==========================================================
// 5. 事件总管：处理扫描、连接、收发数据
// ==========================================================
static int blecent_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            if (memcmp(event->disc.addr.val, TARGET_MAC, 6) == 0) {
                ESP_LOGI(TAG, "发现极空 BMS，准备连接！");
                ble_gap_disc_cancel(); 
                ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr, 30000, NULL, blecent_gap_event, NULL);
            }
            break;

        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "物理连接成功！开始寻找服务 FFE0...");
                g_conn_handle = event->connect.conn_handle;
                ble_gattc_disc_svc_by_uuid(g_conn_handle, &SVC_UUID.u, on_svc_discovered, NULL);
            } else {
                ESP_LOGE(TAG, "连接失败，重启扫描");
                blecent_scan();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "连接断开，重启扫描...");
            g_conn_handle = 0;
            g_write_handle = 0;
            g_notify_handle = 0;
            blecent_scan();
            break;

        case BLE_GAP_EVENT_NOTIFY_RX:
            if (event->notify_rx.attr_handle == g_notify_handle) {
                struct os_mbuf *om = event->notify_rx.om;
                uint16_t len = OS_MBUF_PKTLEN(om);
                uint8_t *data = om->om_data;

                /* 如果没有正在处理数据，且收到了包含包头的 0x02 帧首部 */
                if (!data_handling && !data_recving && len >= 5 && 
                    data[0] == 0x55 && data[1] == 0xAA && data[2] == 0xEB && data[3] == 0x90 && data[4] == 0x02) {
                    
                    rx_len = 0; 
                    data_recving = 1;
                }
                
                /* 将收到的碎片拼接到大缓存中 */
                if (data_recving && (rx_len + len <= RX_BUF_SIZE)) {
                    memcpy(rx_buf + rx_len, data, len);
                    rx_len += len;
                }

                /* 验证是否拼齐了一帧 300 字节，并且是 0x02 帧 */
                if (rx_len >= 300 && rx_buf[0] == 0x55 && rx_buf[1] == 0xAA && rx_buf[4] == 0x02) {
                    data_handling = 1; // 上锁
                    data_recving = 0;  // 接收标志复位
                    
                    // 进行解析
                    task_data_handle(rx_buf);
                    
                    rx_len = 0; // 清空长度，准备下一次接收
                }
            }
            break;

        default:
            break;
    } // 修复：正确闭合了 switch
    return 0;
}

// ==========================================================
// 6. 底层初始化与扫描启动
// ==========================================================
static void blecent_scan(void) {
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    ble_hs_id_infer_auto(0, &own_addr_type);

    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    ESP_LOGI(TAG, "开始扫描极空 BMS...");
    ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params, blecent_gap_event, NULL);
}

static void blecent_on_sync(void) {
    ESP_LOGI(TAG, "蓝牙协议栈准备就绪！");
    blecent_scan();
}

static void blecent_on_reset(int reason) {
    ESP_LOGE(TAG, "蓝牙复位: %d", reason);
}

void blecent_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void send_cmd_to_bms(uint8_t *cmd, uint16_t len) 
{
    if (g_conn_handle != 0 && g_write_handle != 0) {
        ble_gattc_write_no_rsp_flat(g_conn_handle, g_write_handle, cmd, len);
        ESP_LOGI(TAG, "已向 BMS 的【写入】通道发送请求指令！");
    } else {
        ESP_LOGW(TAG, "蓝牙未连接或写入特征未就绪，无法发送指令！");
    }
}

int BLE_Init(void) {
    nvs_flash_init();
    nimble_port_init();

    ble_hs_cfg.reset_cb = blecent_on_reset;
    ble_hs_cfg.sync_cb = blecent_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    nimble_port_freertos_init(blecent_host_task);
    return 0;
}