#include "st7789.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"   /* CMSIS-RTOS v2：提供 osKernelGetTickCount（RTOS 节拍计数） */

#ifndef LV_HOR_RES_MAX
#define LV_HOR_RES_MAX 240
#endif
#ifndef LV_VER_RES_MAX
#define LV_VER_RES_MAX 240
#endif

/* ST7789 控制脚：软件 GPIO 输出，非 SPI 硬件 NSS（hspi1.Init.NSS = SPI_NSS_SOFT）。
 * ⚠ 这里固化的是你板子实际焊接的脚位，必须一一对应！改脚只动这几行，不用碰驱动。 */
#define ST7789_CS_PORT   GPIOB
#define ST7789_CS_PIN    GPIO_PIN_0
#define ST7789_DC_PORT   GPIOB
#define ST7789_DC_PIN    GPIO_PIN_1
#define ST7789_RST_PORT  GPIOB
#define ST7789_RST_PIN   GPIO_PIN_2
#define ST7789_BLK_PORT  GPIOB
#define ST7789_BLK_PIN   GPIO_PIN_10

/* hspi1 定义在 main.c（MX_SPI1_Init 内联），本文件直接引用其句柄 */
extern SPI_HandleTypeDef hspi1;



//PARTIAL渲染模式，脏区更改；240 为实际屏宽，20 行局部刷（省 RAM，原 40 行在 128KB 普通 RAM 下溢出 .bss）
static lv_color_t disp_buf[240*20];



//lvgl刷新回调函数，将脏区推到屏幕更新
static void disp_flush_cb(lv_display_t *disp,const lv_area_t *area,uint8_t *px_map){
    ST7789_Flush(area->x1, area->y1, area->x2, area->y2,
                (lv_color_t *)px_map);

    lv_display_flush_ready(disp);
}

void lv_port_disp_init(void){
    /* ---- ST7789 硬件初始化 ----
     * 注意：ST7789_Init() 内部【不】初始化 GPIO，只保存脚位就直接操作引脚。
     * 所以必须先把这些控制脚配成推挽输出、并开 GPIOB 时钟，否则 hard_reset /
     * 写命令都是对未初始化引脚操作，屏必然不亮。 */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    {
        GPIO_InitTypeDef gpio = {0};
        gpio.Mode  = GPIO_MODE_OUTPUT_PP;
        gpio.Pull  = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        gpio.Pin   = ST7789_CS_PIN;
        HAL_GPIO_Init(ST7789_CS_PORT, &gpio);
        gpio.Pin   = ST7789_DC_PIN;
        HAL_GPIO_Init(ST7789_DC_PORT, &gpio);
        gpio.Pin   = ST7789_RST_PIN;
        HAL_GPIO_Init(ST7789_RST_PORT, &gpio);
        gpio.Pin   = ST7789_BLK_PIN;
        HAL_GPIO_Init(ST7789_BLK_PORT, &gpio);
        HAL_GPIO_WritePin(ST7789_CS_PORT, ST7789_CS_PIN, GPIO_PIN_SET); /* CS 默认高，避免误选 */
    }

    ST7789_Init(&hspi1,
                ST7789_CS_PORT,  ST7789_CS_PIN,
                ST7789_DC_PORT,  ST7789_DC_PIN,
                ST7789_RST_PORT, ST7789_RST_PIN);
    ST7789_SetBacklightPin(ST7789_BLK_PORT, ST7789_BLK_PIN);
    ST7789_Backlight(1);   /* 拉高背光 -> 屏亮（若 BLK 已硬件接 3.3V 常亮，此句无害） */

    lv_display_t * disp=lv_display_create(LV_HOR_RES_MAX,LV_VER_RES_MAX);
    lv_display_set_flush_cb(disp,disp_flush_cb);

    lv_display_set_buffers(disp,
                       disp_buf,
                       NULL,
                       sizeof(disp_buf),
                       LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* 方案1：FreeRTOS 接管 SysTick 后不会调 HAL_IncTick -> HAL_GetTick 永远不前进 ->
     * LVGL 心跳冻结、整屏不刷新。改用 RTOS 内核节拍计数（osKernelGetTickCount），
     * 它由调度器驱动必然前进，LVGL 刷新心跳即恢复。 */
    lv_tick_set_cb(osKernelGetTickCount);
}


