#include "lvgl.h"
#include "ui.h"
#include "bmsdata.h"  // 确保里面有 extern bms_data_t g_bms_data;

// 定义指向页面中 Label 的指针，方便定时器更新文本
static lv_obj_t * label_soc;
static lv_obj_t * label_rem_cap;
static lv_obj_t * label_tot_cap;

static lv_obj_t * label_vol;
static lv_obj_t * label_diff;
static lv_obj_t * label_temp;
static lv_obj_t * label_cycle;


LV_IMG_DECLARE(soc);
LV_IMG_DECLARE(volt);
LV_IMG_DECLARE(diff);
LV_IMG_DECLARE(temp);
LV_IMG_DECLARE(cycle);

// ==========================================
// 动画回调函数：动态改变眼睛的高度来实现“眨眼”
// ==========================================
static void eye_anim_cb(void * var, int32_t v)
{
    lv_obj_set_height((lv_obj_t *)var, v);
}

// ==========================================
// 自动轮播定时器回调：控制页面自动切换和停留时间
// ==========================================
static void auto_scroll_timer_cb(lv_timer_t * timer)
{
    // 获取传递进来的 Tileview 对象
    lv_obj_t * tv = (lv_obj_t *)timer->user_data;
    
    // 静态变量记录当前在第几页 (0:第一页, 1:第二页, 2:第三页)
    static uint8_t current_page = 0; 

    // 页面索引加 1，如果超过 2 就回到 0
    current_page++;
    if (current_page > 2) {
        current_page = 0;
    }

    // 触发滑动动画，跳转到指定页面 (列0, 行current_page)
    lv_obj_set_tile_id(tv, 0, current_page, LV_ANIM_ON);

    // 动态调整下一次定时器触发的时间（即下一页的停留时间）
    if (current_page == 0) {
        lv_timer_set_period(timer, 15000); // 停留在第一页时，等 15 秒后再切走
    } else {
        lv_timer_set_period(timer, 5000);  // 停留在第二、第三页时，等 5 秒后切走
    }
}

// 定时器回调函数：负责把 BLE 接收到的数据刷新到屏幕上
static void ui_update_timer_cb(lv_timer_t * timer)
{
    if (g_bms_data.is_updated) {
        // 刷新第二页 
        lv_label_set_text_fmt(label_soc, "%d %%", g_bms_data.soc);
        lv_label_set_text_fmt(label_rem_cap, "REM: %.2f AH", g_bms_data.remain_cap);
        lv_label_set_text_fmt(label_tot_cap, "MAX: %.2f AH", g_bms_data.total_cap);
        
        // 刷新第三页
        lv_label_set_text_fmt(label_vol, "%.2f V", g_bms_data.total_vol);
        lv_label_set_text_fmt(label_diff, "%.3f V", g_bms_data.volt_diff);
        lv_label_set_text_fmt(label_temp, "%.1f C", g_bms_data.temperature);
        lv_label_set_text_fmt(label_cycle, "%lu", g_bms_data.cycle_count);
        
        g_bms_data.is_updated = false;
    }
}

void app_ui_init(void)
{
    // 1. 创建基于全屏的 Tileview
    lv_obj_t * tv = lv_tileview_create(lv_scr_act());
    lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);

    // ==========================================
    // 强制覆盖暗黑主题：黑底白字
    // ==========================================
    lv_obj_set_style_bg_color(tv, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_color(tv, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    // ==========================================
    // 页面 1：白色大眼睛与眨眼动画
    // ==========================================
    lv_obj_t * tile1 = lv_tileview_add_tile(tv, 0, 0, LV_DIR_BOTTOM);    
    // 左眼
    lv_obj_t * left_eye = lv_obj_create(tile1);
    lv_obj_remove_style_all(left_eye);
    lv_obj_set_size(left_eye, 40, 60);
    lv_obj_align(left_eye, LV_ALIGN_CENTER, -40, 0);
    lv_obj_set_style_radius(left_eye, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(left_eye, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(left_eye, LV_OPA_COVER, LV_PART_MAIN);

    // 右眼
    lv_obj_t * right_eye = lv_obj_create(tile1);
    lv_obj_remove_style_all(right_eye);
    lv_obj_set_size(right_eye, 40, 60);
    lv_obj_align(right_eye, LV_ALIGN_CENTER, 40, 0);
    lv_obj_set_style_radius(right_eye, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(right_eye, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(right_eye, LV_OPA_COVER, LV_PART_MAIN);

    // 创建眨眼动画逻辑
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, eye_anim_cb); 
    lv_anim_set_values(&a, 60, 4);        
    lv_anim_set_time(&a, 150);            
    lv_anim_set_playback_time(&a, 150);   
    lv_anim_set_repeat_delay(&a, 3000);   
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE); 

    lv_anim_set_var(&a, left_eye);
    lv_anim_start(&a);
    lv_anim_set_var(&a, right_eye);
    lv_anim_start(&a);

    // ==========================================
    // 页面 2：容量数据页 
    // ==========================================
    lv_obj_t * tile2 = lv_tileview_add_tile(tv, 0, 1, LV_DIR_TOP | LV_DIR_BOTTOM);

    lv_obj_t * soc_img = lv_img_create(tile2);
    lv_img_set_src(soc_img, &soc);
    lv_obj_set_pos(soc_img, 30, 30);
    
    label_soc = lv_label_create(tile2);
    lv_label_set_text(label_soc, "-- %");
    lv_obj_set_pos(label_soc, 140, 40);
    lv_obj_set_style_text_font(label_soc, &lv_font_montserrat_40, LV_PART_MAIN);

    label_rem_cap = lv_label_create(tile2);
    lv_label_set_text(label_rem_cap, "REM: -- AH");
    lv_obj_set_pos(label_rem_cap, 10, 120);
    lv_obj_set_style_text_font(label_rem_cap, &lv_font_montserrat_32, LV_PART_MAIN);
    
    label_tot_cap = lv_label_create(tile2);
    lv_label_set_text(label_tot_cap, "MAX: -- AH");
    lv_obj_set_pos(label_tot_cap, 10, 180);
    lv_obj_set_style_text_font(label_tot_cap, &lv_font_montserrat_32, LV_PART_MAIN);

    // ==========================================
    // 页面 3：电压与状态数据 
    // ==========================================
    lv_obj_t * tile3 = lv_tileview_add_tile(tv, 0, 2, LV_DIR_TOP);
    lv_obj_set_scrollbar_mode(tile3, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t * volt_img = lv_img_create(tile3);
    lv_img_set_src(volt_img, &volt);
    lv_obj_set_pos(volt_img, 30, 0);
    label_vol = lv_label_create(tile3);
    lv_label_set_text(label_vol, "-- V");
    lv_obj_set_pos(label_vol, 110, 10);
    lv_obj_set_style_text_font(label_vol, &lv_font_montserrat_32, LV_PART_MAIN);
    
    lv_obj_t * diff_img = lv_img_create(tile3);
    lv_img_set_src(diff_img, &diff);
    lv_obj_set_pos(diff_img, 30, 60);
    label_diff = lv_label_create(tile3);
    lv_label_set_text(label_diff, "-- V");
    lv_obj_set_pos(label_diff, 110, 70);
    lv_obj_set_style_text_font(label_diff, &lv_font_montserrat_32, LV_PART_MAIN);
    
    lv_obj_t * temp_img = lv_img_create(tile3);
    lv_img_set_src(temp_img, &temp);
    lv_obj_set_pos(temp_img, 30, 120);
    label_temp = lv_label_create(tile3);
    lv_label_set_text(label_temp, "-- C");
    lv_obj_set_pos(label_temp, 110, 130);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_32, LV_PART_MAIN);
    
    lv_obj_t * cycle_img = lv_img_create(tile3);
    lv_img_set_src(cycle_img, &cycle);
    lv_obj_set_pos(cycle_img, 30, 180);
    label_cycle = lv_label_create(tile3);
    lv_label_set_text(label_cycle, "--");
    lv_obj_set_pos(label_cycle, 110, 190);
    lv_obj_set_style_text_font(label_cycle, &lv_font_montserrat_32, LV_PART_MAIN);

    // ==========================================
    // 4. 启动定时器
    // ==========================================
    // 数据刷新定时器 (200ms)
    lv_timer_create(ui_update_timer_cb, 200, NULL);
    
    // 【新增】：页面自动轮播定时器 
    // 初始等待 15000 毫秒（15秒）后触发第一次滚动，并将 tv 作为参数传给它
    lv_timer_create(auto_scroll_timer_cb, 15000, tv);
}