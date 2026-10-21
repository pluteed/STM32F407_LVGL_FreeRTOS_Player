/*==============================================================================
 * rtos_tasks_reference.c —— 【CMSIS-RTOS v2 版】真实多任务实现
 *
 * 为什么不用"单任务 superloop"：
 *   1) 事件驱动：proto_rx_task 阻塞在【任务通知】上，中断一到立即唤醒；
 *      空闲时 0% CPU 占用，不是靠 osDelay 定时去摸环形缓冲。
 *   2) 串行化发送：PROTO_Send 可能被多个任务并发调用（gui 里点歌、rx 里自动切歌），
 *      都走同一个 huart，HAL_UART_Transmit 会返回 HAL_BUSY 丢帧。
 *      用消息队列把发送收敛到单一任务，天然串行。
 *   3) 并发保护：LVGL 非线程安全，rx_task 收到歌单帧要改列表 UI，
 *      必须和 gui_task 抢同一个互斥量。
 *
 * 与 host_main.c 的关系：
 *   host_main.c 是【裸 FreeRTOS API】写法（xTaskCreate + vTaskStartScheduler），
 *   本文件是同一套任务模型的【CMSIS-RTOS v2】写法（osThreadNew），
 *   专门给你的 CubeMX 工程用。任务划分/优先级/职责完全对齐，只是换了 API 皮。
 *
 * 接线位置（都在 main.c 的 USER CODE 块内，CubeMX 重新 Generate 不会冲掉）：
 *   RTOS_MUTEX   块：HostTasks_Init()
 *   RTOS_QUEUES  块：（空，队列在 HostTasks_Init 里建）
 *   RTOS_THREADS 块：osThreadNew(...) ×3
 *   StartDefaultTask 实体：改成 gui_task_body()
 *============================================================================*/
#include "rtos_tasks_reference.h"

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"          /* vTaskNotifyGiveFromISR / ulTaskNotifyTake */

#include "lvgl.h"
#include "lv_port_disp.h"
#include "st7789.h"            /* ST7789_FillColor：整屏填色自证刷屏通路 */
#include "indev_port.h"
#include "app_framework.h"
#include "proto_f407_port.h"
#include "bgm_service.h"
#include "music_app.h"       //music_list_bootload
#include "main.h"

/* huart1 目前定义在 main.c（.ioc 还没 Generate 出 usart.c）；等 Generate 之后
 * 它会搬到 usart.c，并由 usart.h 提供 extern —— 那时这行与本行重复声明，合法无害，
 * 可以直接删掉本行。 */
extern UART_HandleTypeDef huart1;

/*==============================================================================
 * 一、内核对象（全局，main.c 与中断都要引用）
 *============================================================================*/

/* LVGL 互斥量（Mutex | Mutual Exclusion | 互斥）：
 * 属性开 osMutexPrioInherit（优先级继承，cmsis_os2.h 里名字无 ing）——防止低优先级的 gui_task 拿着锁时，
 * 被中优先级的任务插队，导致高优先级 rx_task 无限等（优先级反转）。 */
const osMutexAttr_t g_lvgl_mutex_attr = {
    .name      = "lvglMutex",
    .attr_bits = osMutexPrioInherit,   /* 注意：cmsis_os2.h 里是 osMutexPrioInherit（无 ing），原拼写错 */
};
osMutexId_t g_lvgl_mutex = NULL;

/* 发送队列（生产者-消费者模型）：
 * 元素 = 整帧副本 proto_tx_item_t；深度 8。
 * 占用 RAM = 8 × (2 + PROTO_MAX_FRAME) ≈ 8 × 269 ≈ 2.2 KB，F407(192KB RAM) 无压力。 */
const osMessageQueueAttr_t g_tx_queue_attr = { .name = "txQueue" };
osMessageQueueId_t g_tx_queue = NULL;

/* 任务句柄 */
osThreadId_t g_gui_task_handle    = NULL;
osThreadId_t g_rx_task_handle     = NULL;
osThreadId_t g_tx_task_handle     = NULL;
osThreadId_t g_input_task_handle  = NULL;

/*==============================================================================
 * 二、任务属性（栈单位是【字节】，注意 CubeMX 默认 128*4=512 太小）
 *============================================================================*/
const osThreadAttr_t gui_task_attributes = {
    .name       = "gui",
    .stack_size = 16384,                   /* ★ 16KB：LVGL v9 首帧渲染栈深，8KB 仍疑似溢出踩坏内核全局(uxDeletedTasksWaitingCleanUp)→空闲任务死转；堆已提到 48KB 装得下 */
    .priority   = (osPriority_t)osPriorityNormal,
};
const osThreadAttr_t rx_task_attributes = {
    .name       = "prx",
    .stack_size = 1024,                    /* 解析 + 分发，会调 App 的 rx 钩子 */
    .priority   = (osPriority_t)osPriorityAboveNormal,  /* 收协议帧优先于刷屏 */
};
const osThreadAttr_t tx_task_attributes = {
    .name       = "ptx",
    .stack_size = 512,
    .priority   = (osPriority_t)osPriorityNormal,
};
const osThreadAttr_t input_task_attributes = {
    .name       = "input",
    .stack_size = 512,
    .priority   = (osPriority_t)osPriorityBelowNormal,
};

/*==============================================================================
 * 三、任务实体
 *============================================================================*/

/*---------- gui_task：LVGL 心跳 + 当前 App 的 tick ----------*/
void gui_task_body(void *argument)
{
    (void)argument;

    /* 外设与 UI 初始化都放在本任务里，确保栈是 4 KB 的那一份。
     * ⚠ 若 lv_port_disp_init() 内部已调过 lv_init()，就把下面这行删掉：
     *   LVGL v9 的 lv_init() 没有防重入保护，重复调用会重新分配内存池，
     *   表现为启动后一段时间莫名 HardFault 或控件画不出来。 */
    lv_init();
    lv_port_disp_init();      /* 建 ST7789 显示；内部已 ST7789_Init + 拉高背光 */
    indev_port_init();        /* 5 向键 keypad */
    app_framework_init();     /* 建 Launcher 首页 */

    //开机拉一次歌单，内部会等下位机第一帧 STATUS，最多阻塞 3 秒
    music_list_bootload();

    for (;;)
    {
        osMutexAcquire(g_lvgl_mutex, osWaitForever);  /* 多任务共享 LVGL 对象，渲染期持锁 */
        lv_timer_handler();        /* LVGL 心跳：刷屏 / 跑动画 / 派发输入事件 */
        app_tick_active();         /* 转发给当前活跃 App 的 tick */
        osMutexRelease(g_lvgl_mutex);

        osDelay(5);           /* ~200 Hz 刷新，够顺滑 */
    }
}

/*---------- proto_rx_task：被中断【通知】唤醒，解析并分发 ----------*/
void proto_rx_task(void *argument)
{
    (void)argument;

    for (;;)
    {
        /* 阻塞等待通知：没有数据时任务挂起，CPU 0% 占用。
         * pdTRUE = 退出前把通知值清零（当作二值信号量用）。
         * 若 Poll 期间又来了新字节，通知会挂起，下一轮 Take 立即返回再排空，不会丢。 */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* 持锁再分发：Dispatch 链末端会碰 LVGL 对象
         * （bgm_on_event 的回调改播放态、app_dispatch_rx 增删 lv_list 按钮）。
         * 持锁粒度 = "排空一次环形缓冲"，最多处理 2 帧（缓冲 534 B ≈ 2 帧），
         * 即 2 ms 内必释放，gui 最多卡一帧刷新。
         * ⚠ 约定：App 的 rx 钩子内部【不许再 Acquire g_lvgl_mutex】，
         *   否则非递归互斥量会自锁死。 */
        osMutexAcquire(g_lvgl_mutex, osWaitForever);
        ProtoF407_Poll();     /* 逐字节喂解析机，收满帧自动 Dispatch */
        osMutexRelease(g_lvgl_mutex);
    }
}

/*---------- proto_tx_task：从队列取帧，真正走 USART 发出 ----------*/
void proto_tx_task(void *argument)
{
    proto_tx_item_t item;
    (void)argument;

    for (;;)
    {
        if (osMessageQueueGet(g_tx_queue, &item, NULL, osWaitForever) == osOK)
        {
            /* 唯一调用 HAL_UART_Transmit 的地方 —— 天然串行，不会抢 huart */
            HAL_UART_Transmit(&huart1, item.buf, item.len, HAL_MAX_DELAY);
        }
    }
}

/*---------- input_task：保留扩展位 ----------*/
void input_task(void *argument)
{
    (void)argument;
    for (;;)
    {
        /* 5 向键已走 LVGL keypad indev（indev_port.c 的 read_cb 由 lv_timer_handler
         * 周期调用），无需在此扫 GPIO。
         * 本任务留作：看门狗喂狗 / SD 卡在位检测 / 后期加编码器或触摸时启用。
         * 确认用不上可以直接删掉这个任务。 */
        osDelay(100);
    }
}

/*==============================================================================
 * 四、初始化入口（在 main.c 的 RTOS_MUTEX 块里调用一次）
 *============================================================================*/
void HostTasks_Init(void)
{
    /* 1) 内核对象必须在建任务之前建好 */
    g_lvgl_mutex = osMutexNew(&g_lvgl_mutex_attr);
    g_tx_queue   = osMessageQueueNew(TX_QUEUE_LEN,
                                     sizeof(proto_tx_item_t),
                                     &g_tx_queue_attr);

    /* 2) 协议端口接线：注册 TX hook（改为投队列）+ 开 RXNE 接收中断 */
    ProtoF407_Init(&huart1);
    ProtoF407_SetTxQueue(g_tx_queue);

    /* 3) 建任务。
     * ⚠ gui 任务【不复用 CubeMX 的 defaultTask】：defaultTask_attributes 定义在
     *   USER CODE 块外，CubeMX 一 Generate 就覆盖回 512 B，跑 LVGL 必爆栈。
     *   这里用本文件的 gui_task_attributes（4096 B）另建，栈大小与 CubeMX 无关。
     *   main.c 里的 StartDefaultTask 改成直接 osThreadTerminate(NULL) 自杀。
     * 顺序：先建 gui（它会做 UI 初始化），再建 prx/ptx/input。 */
    g_gui_task_handle   = osThreadNew(gui_task_body, NULL, &gui_task_attributes);

    g_tx_task_handle    = osThreadNew(proto_tx_task, NULL, &tx_task_attributes);
    g_input_task_handle = osThreadNew(input_task,    NULL, &input_task_attributes);

    g_rx_task_handle    = osThreadNew(proto_rx_task, NULL, &rx_task_attributes);
    ProtoF407_BindRxTask((TaskHandle_t)g_rx_task_handle);  /* 中断拿到句柄才能 Give */
}

/*==============================================================================
 * 五、LVGL 并发保护约定（写 App 时必须遵守）
 *
 *   gui_task：自己持锁跑 lv_timer_handler
 *   其他任务（rx_task / 定时器回调）只要碰任何 lv_* 对象，必须先 Acquire：
 *
 *       osMutexAcquire(g_lvgl_mutex, osWaitForever);
 *       lv_label_set_text(...);
 *       osMutexRelease(g_lvgl_mutex);
 *
 *   反例（必崩）：rx_task 里直接 app_dispatch_rx() 改 lv_list，不持锁，
 *   与 gui_task 的 lv_timer_handler 并发踩同一块 LVGL 内存。
 *============================================================================*/
