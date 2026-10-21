#ifndef __INDEV_PORT_H
#define __INDEV_PORT_H

#include "lvgl.h"
#include "stm32f4xx_hal.h"

void indev_port_init(void);
lv_group_t * indev_port_get_group(void);

#endif