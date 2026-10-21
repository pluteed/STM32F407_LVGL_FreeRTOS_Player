#ifndef __APP_FRAMEWORK_H
#define __APP_FRAMEWORK_H

#include "lvgl.h"

typedef struct {
    const char          *name;   /* 启动器上显示的名字 */
    const lv_img_dsc_t  *icon;   /* 图标（可 NULL，退化为文字按钮） */
    void (*create)(void);        /* 建界面、注册协议 rx 钩子、放 BGM */
    void (*destroy)(void);       /* 清界面、停 BGM */
    void (*tick)(void);          /* gui_task 每轮调用，做周期刷新 */
} app_entry_t;

void app_framework_init(void);                 /* 建启动器首页 */
void app_launch(uint8_t idx);                  /* 切换应用 */
void app_go_home(void);                         /* 从任意 App 返回 Launcher（方案 A） */
uint8_t app_count(void);
uint8_t app_active_idx(void);
void app_tick_active(void);                    /* gui_task 调它，转发给当前 App 的 tick */
void app_set_rx_handler(void (*cb)(const void *frame)); /* 当前 App 收协议帧的钩子 */
void app_dispatch_rx(const void *frame);       /* 协议口收到整帧后转给当前 App */

#endif