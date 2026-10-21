/*==============================================================================
 * proto_f407_port.c —— 协议层 F407 移植实现（自包含版，无 FreeRTOS 队列依赖）
 *
 * 移植清单（按你原理图填空）：
 *   - 用哪路 USART 接 RCT6：默认 USART1，Init 时把 &huart1 传进来即可
 *   - 在 stm32f4xx_it.c 找到 USART1_IRQHandler，函数体里加一句：
 *         ProtoF407_IRQ(&huart1);
 *     放在 HAL_UART_IRQHandler 之前或之后都行（这里裸读 DR，不依赖 HAL 接收流程）
 *   - 确认 RCT6 那侧波特率/字长/停止位与 F407 一致（协议里约定 115200/8N1）
 *   - .ioc 里把 USART1 配成 Asynchronous，并生成 MX_USART1_UART_Init()
 *
 * ★吊毛改：线程模型已升级为【真 RTOS 版】（2026-10-20，老大指出原方案退化成轮询）
 *   - ProtoF407_IRQ 跑在【中断上下文】：只做"读 DR + 入环形缓冲"，然后
 *     vTaskNotifyGiveFromISR 唤醒 prx 任务。绝不解析、绝不碰 LVGL。
 *   - prx 任务阻塞在 ulTaskNotifyTake 上（空闲 0% CPU），被唤醒后持 g_lvgl_mutex
 *     调 ProtoF407_Poll，排空缓冲、喂解析机、收满帧即分发。
 *   - ptx 任务阻塞在发送队列上，是全工程【唯一】调 HAL_UART_Transmit 的地方，
 *     串行化多路 PROTO_Send，避免 huart 的 gState 忙冲突丢帧。
 *   ⚠ 不要在 main 的 while 里定时轮询 Poll —— 那会退化成半轮询，
 *     RTOS 的优先级/阻塞/事件驱动优势全废。
 *============================================================================*/
#include "proto_f407_port.h"
#include "bgm_service.h"
#include "app_framework.h"

/* ★吊毛加：vTaskNotifyGiveFromISR / ulTaskNotifyTake / TaskHandle_t 需要 */
#include "FreeRTOS.h"
#include "task.h"

/*------------------- 内部状态 ----------------*/
static UART_HandleTypeDef *g_huart = NULL;

/* ★吊毛加：RTOS 接线点（由 HostTasks_Init 注入，见 rtos_tasks_reference.c） */
static osMessageQueueId_t g_txq     = NULL;   /* 发送队列；NULL 时退化成阻塞直发 */
static TaskHandle_t       g_rx_task = NULL;   /* prx 任务句柄；NULL 时中断不 Give */

/* 接收环形缓冲（Ring Buffer | 环形缓冲 | 首尾指针环绕的 FIFO）：
 * 单生产者 = UART 中断（写 g_rx_w），单消费者 = Poll 任务（读 g_rx_r），
 * 16 位索引在 Cortex-M 上读写原子，无需关中断。 */
#define RX_BUF_LEN  (PROTO_MAX_FRAME * 2)
static uint8_t  g_rxb[RX_BUF_LEN];
static volatile uint16_t g_rx_w = 0;   /* 写指针（中断更新） */
static volatile uint16_t g_rx_r = 0;   /* 读指针（任务更新） */

/*------------------- TX hook：PROTO_Send 会回调它 ----------------*/
static int8_t tx_hook(const uint8_t *buf, uint16_t len) {
    uint16_t i;
    if (g_huart == NULL) return -1;                 /* 还没 Init，发不出去 */
    if (len > PROTO_MAX_FRAME) len = PROTO_MAX_FRAME;

    /* ★吊毛改：优先投队列，由 ptx 任务统一发出。
     * 原因：PROTO_Send 会被多个任务并发调用（gui 里点歌、prx 里自动切歌…），
     * 都走同一个 huart；直接调 HAL_UART_Transmit 会因 gState 忙返回 HAL_BUSY 丢帧。
     * 收敛到单一发送任务后天然串行。 */
    if (g_txq != NULL) {
        proto_tx_item_t item;
        item.len = len;
        for (i = 0; i < len; i++) item.buf[i] = buf[i];   /* 拷副本，防调用者栈失效 */

        /* 超时 10ms：队列满说明发送端堵了，宁可丢这一帧也别把调用者卡死 */
        if (osMessageQueuePut(g_txq, &item, 0U, 10U) == osOK) return 0;
        return -1;
    }

    /* 退化路径：还没建队列（初始化早期）时阻塞直发 */
    HAL_UART_Transmit(g_huart, (uint8_t *)buf, len, HAL_MAX_DELAY);
    return 0;
}

/* ★吊毛加：注入发送队列 + 绑定 prx 任务句柄（HostTasks_Init 里调） */
void ProtoF407_SetTxQueue(osMessageQueueId_t q) { g_txq = q; }
void ProtoF407_BindRxTask(TaskHandle_t h)       { g_rx_task = h; }

/*------------------- 初始化 ----------------*/
void ProtoF407_Init(UART_HandleTypeDef *huart) {
    g_huart = huart;
    PROTO_SetTxHook(tx_hook);    /* 让 PROTO_Send 真正能从 USART 发出去 */
    PROTO_ParserInit();          /* 复位字节级解析状态机 */
    __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);   /* 裸开接收中断，不依赖 HAL_Receive_IT */
}

/*------------------- 中断入口：只搬字节，不解析 ----------------*/
void ProtoF407_IRQ(UART_HandleTypeDef *huart) {
    BaseType_t woken = pdFALSE;
    uint16_t   got   = 0;

    /* ★吊毛改：一次中断里可能积压多字节，改成 while 尽量搬空 */
    while (__HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE) != RESET) {
        uint8_t  b    = (uint8_t)huart->Instance->DR;
        uint16_t next = (uint16_t)((g_rx_w + 1) % RX_BUF_LEN);
        if (next != g_rx_r) {           /* 缓冲没满才写，满了丢弃最旧 */
            g_rxb[g_rx_w] = b;
            g_rx_w = next;
            got++;
        }
    }

    /* ★吊毛加：唤醒 prx 任务。ISR 里只 Give，绝不做解析、绝不碰 LVGL。
     * 任务通知（Task Notification）比队列/二值信号量更轻：无需内核对象内存，
     * 官方实测唤醒快约 45%，单生产者单消费者（本场景正是）下是首选。
     * 一次中断只 Give 一次（通知值 +1），任务侧 Take 一次后把环形缓冲排空，
     * 不会因为搬了 N 字节就 Give N 次导致通知值无谓累积。 */
    if ((got > 0) && (g_rx_task != NULL)) {
        vTaskNotifyGiveFromISR(g_rx_task, &woken);
        portYIELD_FROM_ISR(woken);   /* 若被唤醒任务优先级更高，退出中断即刻切换 */
    }
    /* 注意：不要用 HAL_UART_IRQHandler，那里会清标志并走 HAL 接收回调，
     * 与本裸读 DR 冲突。若你工程里 USART1 还接了别的 HAL 接收，需协调。 */
}

/*------------------- 任务里循环调用：取字节 -> 喂解析机 -> 分发 ----------------*/
void ProtoF407_Poll(void) {
    while (g_rx_r != g_rx_w) {
        uint8_t b = g_rxb[g_rx_r];
        g_rx_r = (uint16_t)((g_rx_r + 1) % RX_BUF_LEN);

        int8_t r = PROTO_FeedByte(b);
        if (r == 1) {
            ProtoF407_Dispatch(&g_rx_frame);   /* 收满一整帧 */
        }
        /* r == 0 ：收集中，继续；r < 0 ：校验失败，状态机已自复位，继续 */
    }
}

/*------------------- 收满一帧后分流 ----------------*/
void ProtoF407_Dispatch(const proto_frame_t *f) {
    if (f->cmd == CMD_EVENT) {
        /* 事件帧：第一字节是事件类型，后续是载荷 */
        uint8_t  evt  = f->pay[0];
        uint16_t plen = (f->len > 0) ? (uint16_t)(f->len - 1) : 0;
        bgm_on_event(evt, f->pay + 1, plen);
    }
    /* 其余（STATUS / LIST_BEGIN/ITEM/END 等）一律交给当前活跃 App 的 rx 钩子。
     * 没有活跃 App（Launcher 态）时 app_dispatch_rx 内部 g_app_rx 为 NULL，静默跳过。 */
    app_dispatch_rx(f);
}
