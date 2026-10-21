#ifndef PROTO_F407_PORT_H
#define PROTO_F407_PORT_H

#include "stm32f4xx_hal.h"
#include "cmsis_os.h"   /* ★吊毛加：osMessageQueueId_t / osThreadId_t 需要 */
#include "proto.h"

/* ★吊毛加：发送队列元素 = 整帧副本。
 * 为什么要副本：PROTO_Send 的调用者（gui / prx 任务）帧数据常在栈上，
 * 投队列后函数返回栈就废了，ptx 任务再读就是野数据。 */
typedef struct {
    uint16_t len;
    uint8_t  buf[PROTO_MAX_FRAME];
} proto_tx_item_t;

#define TX_QUEUE_LEN  8

void ProtoF407_Init(UART_HandleTypeDef *huart);   /* 注册 TX hook + 开 RXNE 中断 */
void ProtoF407_IRQ(UART_HandleTypeDef *huart);    /* 在 UART 中断服务函数里调用 */
void ProtoF407_Poll(void);                         /* 在 proto_rx_task 循环里调用 */
void ProtoF407_Dispatch(const proto_frame_t *f);   /* 收满一帧后调用（Poll 内部用） */

/* ★吊毛加（真 RTOS 版接线点，都在 rtos_tasks_reference.c 的 HostTasks_Init 里调用）：
 *   SetTxQueue —— 让 tx_hook 把帧投进队列，由 ptx 任务统一发出（串行化，防抢 huart）
 *   BindRxTask —— 把 prx 任务句柄交给中断，收到字节后 vTaskNotifyGiveFromISR 唤醒
 *   不调用则退化成"阻塞直发 + 需外部定时轮询 Poll"。 */
void ProtoF407_SetTxQueue(osMessageQueueId_t q);
void ProtoF407_BindRxTask(TaskHandle_t h);

#endif