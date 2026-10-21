#ifndef __ST7789_H
#define __ST7789_H

#include "stm32f4xx_hal.h"
#include "lvgl.h"

/*------------------------------------------------------------------------------
 * 1) 系统功能命令（System Function Command）—— 0x00 ~ 0x0F
 *----------------------------------------------------------------------------*/
#define ST7789_CMD_NOP      0x00   /* No Operation | 空操作 */
#define ST7789_CMD_SWRESET  0x01   /* Software Reset | 软件复位 */
#define ST7789_CMD_RDDID    0x04   /* Read Display ID | 读显示 ID */
#define ST7789_CMD_RDDST    0x09   /* Read Display Status | 读显示状态 */
#define ST7789_CMD_RDDPM    0x0A   /* Read Display Power Mode | 读电源模式 */
#define ST7789_CMD_RDDMADCTL 0x0B  /* Read Display MADCTL | 读显存访问控制 */
#define ST7789_CMD_RDDCOLMOD 0x0C  /* Read Display Pixel Format | 读像素格式 */
#define ST7789_CMD_RDDIM    0x0D   /* Read Display Image Mode | 读图像模式 */
#define ST7789_CMD_RDDSM    0x0E   /* Read Display Signal Mode | 读信号模式 */
#define ST7789_CMD_RDDSDR   0x0F   /* Read Display Self-Diagnostic Result | 读自检结果 */

/*------------------------------------------------------------------------------
 * 2) 睡眠 / 显示模式命令（Sleep & Display Mode）—— 0x10 ~ 0x13
 *----------------------------------------------------------------------------*/
#define ST7789_CMD_SLPIN    0x10   /* Sleep In | 睡眠进入 */
#define ST7789_CMD_SLPOUT   0x11   /* Sleep Out | 退出睡眠 */
#define ST7789_CMD_PTLON    0x12   /* Partial Display Mode On | 局部显示开 */
#define ST7789_CMD_NORON    0x13   /* Normal Display Mode On | 正常显示开 */

/*------------------------------------------------------------------------------
 * 3) 反相 / 空闲 / 伽马 / 开关（Inversion / Idle / Gamma / On-Off）—— 0x20 ~ 0x29
 *----------------------------------------------------------------------------*/
#define ST7789_CMD_INVOFF   0x20   /* Display Inversion Off | 显示反相关 */
#define ST7789_CMD_INVON    0x21   /* Display Inversion On | 显示反相开 */
#define ST7789_CMD_GAMSET   0x26   /* Gamma Set | 伽马曲线选择 */
#define ST7789_CMD_DISPOFF  0x28   /* Display Off | 显示关 */
#define ST7789_CMD_DISPON   0x29   /* Display On | 显示开 */

/*------------------------------------------------------------------------------
 * 4) RAM 访问命令（RAM Access）—— 0x2A ~ 0x3E
 *----------------------------------------------------------------------------*/
#define ST7789_CMD_CASET    0x2A   /* Column Address Set | 列地址设置 */
#define ST7789_CMD_RASET    0x2B   /* Row Address Set | 行地址设置 */
#define ST7789_CMD_RAMWR    0x2C   /* Memory Write | 写显存 */
#define ST7789_CMD_RAMRD    0x2E   /* Memory Read | 读显存 */
#define ST7789_CMD_PTLAR    0x30   /* Partial Area | 局部区域设置 */
#define ST7789_CMD_VSCRDEF  0x33   /* Vertical Scrolling Definition | 垂直滚动定义 */
#define ST7789_CMD_TEOFF    0x34   /* Tearing Effect Line Off | 撕裂线关 */
#define ST7789_CMD_TEON     0x35   /* Tearing Effect Line On | 撕裂线开 */
#define ST7789_CMD_MADCTL   0x36   /* Memory Data Access Control | 显存访问控制(扫描方向) */
#define ST7789_CMD_VSCSAD   0x37   /* Vertical Scroll Start Address | 垂直滚动起始地址 */
#define ST7789_CMD_IDMOFF   0x38   /* Idle Mode Off | 空闲模式关 */
#define ST7789_CMD_IDMON    0x39   /* Idle Mode On | 空闲模式开 */
#define ST7789_CMD_COLMOD   0x3A   /* Interface Pixel Format | 像素格式(色深)设置 */
#define ST7789_CMD_WRMEMC   0x3C   /* Write Memory Continue | 连续写显存 */
#define ST7789_CMD_RDMEMC   0x3E   /* Read Memory Continue | 连续读显存 */

/*------------------------------------------------------------------------------
 * 5) 亮度 / CABC（Brightness & Content Adaptive Brightness Control）—— 0x44 ~ 0x68
 *----------------------------------------------------------------------------*/
#define ST7789_CMD_STE      0x44   /* Set Tear Scanline | 设置撕裂扫描线 */
#define ST7789_CMD_GSCAN    0x45   /* Get Scanline | 读扫描线 */
#define ST7789_CMD_WRDISBV  0x51   /* Write Display Brightness Value | 写亮度 */
#define ST7789_CMD_RDDISBV  0x52   /* Read Display Brightness Value | 读亮度 */
#define ST7789_CMD_WRCTRLD  0x53   /* Write CTRL Display | 写控制显示 */
#define ST7789_CMD_RDCTRLD  0x54   /* Read CTRL Display | 读控制显示 */
#define ST7789_CMD_WRCACE   0x55   /* Write CABC & Color Enhancement | 写自适应亮度/色彩增强 */
#define ST7789_CMD_RDCABC   0x56   /* Read CABC | 读自适应亮度 */
#define ST7789_CMD_WRCABCMB 0x5E   /* Write CABC Minimum Brightness | 写最小亮度 */
#define ST7789_CMD_RDCABCMB 0x5F   /* Read CABC Minimum Brightness | 读最小亮度 */
#define ST7789_CMD_RDABCSDR 0x68   /* Read ABC Self-Diagnostic Result | 读自适应亮度自检 */

/*------------------------------------------------------------------------------
 * 6) 命令集 2（Command Set 2，厂商扩展 / 初始化与电压伽马）—— 0xB0 ~ 0xFE
 *----------------------------------------------------------------------------*/
#define ST7789_CMD_RAMCTRL  0xB0   /* RAM Control | RAM 控制 */
#define ST7789_CMD_RGBCTRL  0xB1   /* RGB Interface Control | RGB 接口控制 */
#define ST7789_CMD_PORCTRL  0xB2   /* Porch Setting | 前后肩设置 */
#define ST7789_CMD_FRCTRL1  0xB3   /* Frame Rate Control 1 | 帧率控制1(局部/空闲) */
#define ST7789_CMD_PARCTRL  0xB5   /* Partial Control | 局部控制 */
#define ST7789_CMD_GCTRL    0xB7   /* Gate Control | 栅极控制 */
#define ST7789_CMD_GTADJ    0xB8   /* Gate On Timing Adjustment | 栅极开启时序 */
#define ST7789_CMD_DGMEN    0xBA   /* Digital Gamma Enable | 数字伽马使能 */
#define ST7789_CMD_VCOMS    0xBB   /* VCOM Setting | VCOM 电压设置 */
#define ST7789_CMD_POWSAVE  0xBC   /* Power Saving Mode | 省电模式 */
#define ST7789_CMD_DLPOFFSAVE 0xBD /* Display Off Power Save | 关显示省电 */
#define ST7789_CMD_LCMCTRL  0xC0   /* LCM Control | LCM 控制 */
#define ST7789_CMD_IDSET    0xC1   /* ID Code Setting | ID 码设置 */
#define ST7789_CMD_VDVVRHEN 0xC2   /* VDV and VRH Command Enable | VDV/VRH 命令使能 */
#define ST7789_CMD_VRHS     0xC3   /* VRH Set | VRH 设置 */
#define ST7789_CMD_VDVS     0xC4   /* VDV Set | VDV 设置 */
#define ST7789_CMD_VCMOFSET 0xC5   /* VCOM Offset Set | VCOM 偏移设置 */
#define ST7789_CMD_FRCTRL2  0xC6   /* Frame Rate Control 2 | 帧率控制2(正常) */
#define ST7789_CMD_CABCCTRL 0xC7   /* CABC Control | 自适应亮度控制 */
#define ST7789_CMD_REGSEL1  0xC8   /* Register Value Selection 1 | 寄存器值选择1 */
#define ST7789_CMD_REGSEL2  0xCA   /* Register Value Selection 2 | 寄存器值选择2 */
#define ST7789_CMD_PWMFRSEL 0xCC   /* PWM Frequency Selection | PWM 频率选择 */
#define ST7789_CMD_PWCTRL1  0xD0   /* Power Control 1 | 电源控制1 */
#define ST7789_CMD_VAPVANEN 0xD2   /* Enable VAP/VAN Signal Output | 启用 VAP/VAN */
#define ST7789_CMD_CMD2EN   0xDF   /* Command 2 Enable | 命令集2使能 */
#define ST7789_CMD_PVGAMCTRL 0xE0  /* Positive Voltage Gamma Control | 正电压伽马控制 */
#define ST7789_CMD_NVGAMCTRL 0xE1  /* Negative Voltage Gamma Control | 负电压伽马控制 */
#define ST7789_CMD_DGMLUTR  0xE2   /* Digital Gamma LUT Red | 数字伽马查找表(红) */
#define ST7789_CMD_DGMLUTB  0xE3   /* Digital Gamma LUT Blue | 数字伽马查找表(蓝) */
#define ST7789_CMD_GATECTRL 0xE4   /* Gate Control | 栅极控制 */
#define ST7789_CMD_SPI2EN   0xE7   /* SPI2 Enable | SPI2 使能 */
#define ST7789_CMD_PWCTRL2  0xE8   /* Power Control 2 | 电源控制2 */
#define ST7789_CMD_EQCTRL   0xE9   /* Equalize Time Control | 均衡时间控制 */
#define ST7789_CMD_PROMCTRL 0xEC   /* Program Mode Control | 编程模式控制 */
#define ST7789_CMD_PROMEN   0xFA   /* Program Mode Enable | 编程模式使能 */
#define ST7789_CMD_NVMSET   0xFC   /* NVM Setting | 非易失存储器设置 */
#define ST7789_CMD_PROMACT  0xFE   /* Program Action | 编程动作 */

/*------------------------------------------------------------------------------
 * 7) 读 ID 命令（Read ID）—— 0xDA ~ 0xDC
 *----------------------------------------------------------------------------*/
#define ST7789_CMD_RDID1    0xDA   /* Read ID1 | 读 ID1 */
#define ST7789_CMD_RDID2    0xDB   /* Read ID2 | 读 ID2 */
#define ST7789_CMD_RDID3    0xDC   /* Read ID3 | 读 ID3 */

/*------------------------------------------------------------------------------
 * 8) 常用参数宏（命令附带的数据值，非命令本身，便于替代裸数）
 *----------------------------------------------------------------------------*/
/* COLMOD(0x3A) 像素格式参数值（高/低 4 位分别为 RGB 接口/CPU 接口格式） */
#define ST7789_COLMOD_12BIT_RGB444   0x03   /* 12 bpp, RGB444 (4-4-4) */
#define ST7789_COLMOD_16BIT_RGB565   0x55   /* 16 bpp, RGB565（LVGL 默认） */
#define ST7789_COLMOD_18BIT_RGB666   0x66   /* 18 bpp, RGB666 */

/* MADCTL(0x36) 位定义（扫描方向 / 颜色顺序） */
#define ST7789_MADCTL_MY   0x80   /* Row Address Order | 行地址顺序 */
#define ST7789_MADCTL_MX   0x40   /* Column Address Order | 列地址顺序 */
#define ST7789_MADCTL_MV   0x20   /* Row/Column Exchange | 行列交换(旋转90°) */
#define ST7789_MADCTL_ML   0x10   /* Vertical Refresh Order | 垂直刷新顺序 */
#define ST7789_MADCTL_RGB  0x00   /* RGB 顺序（默认） */
#define ST7789_MADCTL_BGR  0x08   /* BGR 顺序（颜色发蓝/紫时置此位） */
#define ST7789_MADCTL_MH   0x04   /* Horizontal Refresh Order | 水平刷新顺序 */

/* MADCTL 默认：本 240x240 模块需开 BGR 位(0x08)。
 * 实测 2026-10-20：MADCTL=0x00 时发 0xF800(红)显示天蓝；改 0x08 后仍为偏色，
 * 最终定位颜色错位根因在 ST7789_Flush 字节序(LVGL 小端 vs ST7789 大端)，
 * 已逐像素 swap 修复。MADCTL 保持 0x08(BGR)。 */
#define ST7789_MADCTL_DEFAULT  ST7789_MADCTL_BGR


void ST7789_Init(SPI_HandleTypeDef *hspi,
                 GPIO_TypeDef *cs_port,  uint16_t cs_pin,
                 GPIO_TypeDef *dc_port,  uint16_t dc_pin,
                 GPIO_TypeDef *rst_port, uint16_t rst_pin);

void ST7789_Flush(int32_t x1,int32_t y1,int32_t x2,int32_t y2,const lv_color_t *color_p);

void ST7789_Backlight(uint8_t on);

/* 设置背光控制脚（ST7789_Init 不接收 blk 参数，另用此函数固化，
 * 否则 g_blk_port 为 NULL，ST7789_Backlight 永远 early-return 导致屏黑） */
void ST7789_SetBacklightPin(GPIO_TypeDef *blk_port, uint16_t blk_pin);

/* 整屏单色填充（自证刷屏通路用）：color 为 RGB565 16 位值，
 * 例如纯红 = 0xF800。绕过 LVGL 直接写屏，用于验证 SPI+初始化是否打通。 */
void ST7789_FillColor(uint16_t color);

#endif