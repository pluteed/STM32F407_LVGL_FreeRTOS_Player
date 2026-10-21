#include "indev_port.h"
#include "stm32f4xx_hal.h"   /* 补：HAL_GPIO_ReadPin 需要（原文件漏了这一个头文件） */

#define KEY_PORT     GPIOC
#define KEY_UP       GPIO_PIN_0
#define KEY_DOWN     GPIO_PIN_1
#define KEY_LEFT     GPIO_PIN_2
#define KEY_RIGHT    GPIO_PIN_3
#define KEY_ENTER    GPIO_PIN_4

/* 实体 Home 返回键（可选）：默认关闭(0)。
   有空闲 GPIO 时把 KEY_HOME_EN 改 1 并填下面端口/引脚即可用实体键返回 Launcher；
   框架侧 app_framework.c 已挂好 LV_KEY_HOME 拦截钩子，这里只要上报就会生效。 */
#define KEY_HOME_EN     1
#if KEY_HOME_EN
#define KEY_HOME_PORT   GPIOC
#define KEY_HOME_PIN    GPIO_PIN_5
#endif

static lv_group_t *g_def_group=NULL;

static void indev_read_cb(lv_indev_t *indev,lv_indev_data_t *data){
    (void)indev;
    uint32_t key=0;

#if KEY_HOME_EN   /* Home 优先级最高；这里的 else 与下方 if 串成 else-if 链 */
    if(HAL_GPIO_ReadPin(KEY_HOME_PORT,KEY_HOME_PIN)==GPIO_PIN_RESET){key=LV_KEY_HOME;}
    else
#endif
    /* ★ LVGL v9 keypad 模式：group 焦点移动仅响应 LV_KEY_NEXT/LV_KEY_PREV（Tab/Shift+Tab 语义）；
     *   UP/DOWN/LEFT/RIGHT 默认不移动焦点（只用于对象内部滚动/勾选，普通 lv_btn 不吃 → 焦点卡死）。
     *   故把 4 向键都映射成 PREV/NEXT，让焦点在按钮间循环切换。此前上报 LV_KEY_UP/DOWN 即根因。 */
    if(HAL_GPIO_ReadPin(KEY_PORT,KEY_UP)==GPIO_PIN_RESET){key=LV_KEY_PREV;}
    else if(HAL_GPIO_ReadPin(KEY_PORT,KEY_DOWN)==GPIO_PIN_RESET){key=LV_KEY_NEXT;}
    else if(HAL_GPIO_ReadPin(KEY_PORT,KEY_LEFT)==GPIO_PIN_RESET){key=LV_KEY_PREV;}
    else if(HAL_GPIO_ReadPin(KEY_PORT,KEY_RIGHT)==GPIO_PIN_RESET){key=LV_KEY_NEXT;}
    else if(HAL_GPIO_ReadPin(KEY_PORT, KEY_ENTER)==GPIO_PIN_RESET)key=LV_KEY_ENTER;

    if(key!=0){
        data->state=LV_INDEV_STATE_PRESSED;
        data->key=key;
    }
    else{
        data->state=LV_INDEV_STATE_RELEASED;
    }
}

void indev_port_init(void){
    lv_indev_t *indev=lv_indev_create();
    lv_indev_set_type(indev,LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev,indev_read_cb);

    g_def_group=lv_group_create();
    lv_indev_set_group(indev,g_def_group);
}

lv_group_t * indev_port_get_group(void) {
    return g_def_group;
}
