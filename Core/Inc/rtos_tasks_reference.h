/*==============================================================================
 * rtos_tasks_reference.h —— CMSIS-RTOS v2 版多任务的集中声明
 *
 * proto_f407_port.c（中断里要 Give 通知）与 app_*.c（改 UI 要抢 LVGL 锁）
 * 都 extern 这一份，避免各写各的。
 *============================================================================*/
#ifndef RTOS_TASKS_REFERENCE_H
#define RTOS_TASKS_REFERENCE_H

#include "cmsis_os.h"
#include "proto.h"
#include "proto_f407_port.h"   /* proto_tx_item_t / TX_QUEUE_LEN 定义在这里 */

/* 发送队列里放的是整帧副本（定义见 proto_f407_port.h）：
 * 避免调用者栈上的 buf 被回收后失效。 */

/* ---- 内核对象 ---- */
extern osMutexId_t         g_lvgl_mutex;          /* 保护 LVGL（非线程安全） */
extern osMessageQueueId_t  g_tx_queue;            /* 元素类型 proto_tx_item_t */

/* ---- 任务句柄 ---- */
extern osThreadId_t g_gui_task_handle;
extern osThreadId_t g_rx_task_handle;
extern osThreadId_t g_tx_task_handle;
extern osThreadId_t g_input_task_handle;

/* ---- 任务属性（main.c 建任务时用） ---- */
extern const osThreadAttr_t gui_task_attributes;
extern const osThreadAttr_t rx_task_attributes;
extern const osThreadAttr_t tx_task_attributes;
extern const osThreadAttr_t input_task_attributes;

/* ---- 入口 ---- */
void HostTasks_Init(void);      /* 建内核对象 + 协议接线 + 建 rx/tx/input 任务 */
void gui_task_body(void *argument);   /* gui 主体：由 StartDefaultTask 调用，不返回 */
void proto_rx_task(void *argument);
void proto_tx_task(void *argument);
void input_task(void *argument);

#endif /* RTOS_TASKS_REFERENCE_H */
