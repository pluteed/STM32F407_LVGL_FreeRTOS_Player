#include "st7789.h"

#define ST7789_w 240
#define ST7789_H 240

//AC6优化时自动内联，故不用宏定义，用函数可断点调试
static SPI_HandleTypeDef *g_spi;
static GPIO_TypeDef *g_cs_port;     static uint16_t g_cs_pin;
static GPIO_TypeDef *g_dc_port;     static uint16_t g_dc_pin;
static GPIO_TypeDef *g_rst_port;     static uint16_t g_rst_pin;
static GPIO_TypeDef *g_blk_port;     static uint16_t g_blk_pin;

static void cs_low(void){HAL_GPIO_WritePin(g_cs_port,g_cs_pin,GPIO_PIN_RESET);}
static void cs_high(void){HAL_GPIO_WritePin(g_cs_port,g_cs_pin,GPIO_PIN_SET);}
static void dc_cmd(void){HAL_GPIO_WritePin(g_dc_port,g_dc_pin,GPIO_PIN_RESET);}
static void dc_data(void){HAL_GPIO_WritePin(g_dc_port,g_dc_pin,GPIO_PIN_SET);}

static void write_cmd(uint8_t cmd){
    cs_low(); dc_cmd();
    HAL_SPI_Transmit(g_spi,&cmd,1,HAL_MAX_DELAY);
    cs_high();
}

static void write_data(const uint8_t *d,uint32_t len){
    cs_low(); dc_data();
    while(len > 0){
        uint16_t chunk = (len > 65535U) ? 65535U : (uint16_t)len;
        HAL_SPI_Transmit(g_spi,(uint8_t*)d,chunk,HAL_MAX_DELAY);
        d += chunk;
        len -= chunk;
    }
    cs_high();
}
static void write_u8(uint8_t d) { write_data(&d, 1); }
static void delay_ms(uint32_t ms) { HAL_Delay(ms); }

static void hard_reset(void){
    HAL_GPIO_WritePin(g_rst_port, g_rst_pin, GPIO_PIN_RESET);
    delay_ms(10);
    HAL_GPIO_WritePin(g_rst_port, g_rst_pin, GPIO_PIN_SET);
    delay_ms(120);
}

void ST7789_Init(SPI_HandleTypeDef *hspi,
                 GPIO_TypeDef *cs_port,  uint16_t cs_pin,
                 GPIO_TypeDef *dc_port,  uint16_t dc_pin,
                 GPIO_TypeDef *rst_port, uint16_t rst_pin){
    g_spi = hspi;
    g_cs_port = cs_port; g_cs_pin = cs_pin;
    g_dc_port = dc_port; g_dc_pin = dc_pin;
    g_rst_port = rst_port; g_rst_pin = rst_pin;

    hard_reset();

    write_cmd(ST7789_CMD_SLPOUT); delay_ms(120);
    write_cmd(ST7789_CMD_COLMOD); write_u8(ST7789_COLMOD_16BIT_RGB565);
    write_cmd(ST7789_CMD_MADCTL);  write_u8(ST7789_MADCTL_DEFAULT);       
    //write_cmd(ST7789_CMD_INVON);                                  
    write_cmd(ST7789_CMD_DISPON); 
}

void ST7789_Flush(int32_t x1,int32_t y1,int32_t x2,int32_t y2,const lv_color_t *color_p){
    uint8_t col[4],row[4];
    col[0]=(uint8_t)((x1>>8)&0xFF); col[1]=(uint8_t)(x1&0xFF);
    col[2]=(uint8_t)((x2>>8)&0xFF); col[3]=(uint8_t)(x2&0xFF);
    row[0]=(uint8_t)((y1>>8)&0xFF); row[1]=(uint8_t)(y1&0xFF);
    row[2]=(uint8_t)((y2>>8)&0xFF); row[3]=(uint8_t)(y2&0xFF);

    write_cmd(ST7789_CMD_CASET); write_data(col,4);
    write_cmd(ST7789_CMD_RASET); write_data(row,4);
    write_cmd(ST7789_CMD_RAMWR);

    /* LVGL 的 lv_color_t(RGB565) 在 F4(小端) 内存里低字节在前，而 ST7789
     * RAMWR 要求高字节(D15..D8)先发（大端）。直接当 uint16_t 取像素值，
     * 再按"高字节先发"送出，与 ST7789_FillColor 的发送顺序完全一致。
     * （v9 的 lv_color_t 已是裸 16 位 RGB565，无 .full 成员，勿用 v8 写法） */
    uint32_t n=(uint32_t)(x2-x1+1)*(uint32_t)(y2-y1+1);
    const uint16_t *px16 = (const uint16_t *)color_p;
    static uint8_t sbuf[256 * 2];
    uint32_t done = 0;
    while (done < n) {
        uint16_t chunk = (uint16_t)((n - done) > 256U ? 256U : (n - done));
        for (uint16_t i = 0; i < chunk; i++) {
            uint16_t px = px16[done + i];
            sbuf[2 * i]     = (uint8_t)(px >> 8);      /* 高字节先发 */
            sbuf[2 * i + 1] = (uint8_t)(px & 0xFF);    /* 低字节后发 */
        }
        write_data(sbuf, (uint32_t)(chunk * 2));
        done += chunk;
    }
}

void ST7789_Backlight(uint8_t on){
    if(g_blk_port==NULL) return;
    if(on){HAL_GPIO_WritePin(g_blk_port,g_blk_pin,GPIO_PIN_SET);}
}

void ST7789_SetBacklightPin(GPIO_TypeDef *blk_port, uint16_t blk_pin){
    g_blk_port = blk_port; g_blk_pin = blk_pin;
}

/* 整屏单色填充：CASET/RASET 设满屏 + RAMWR，然后在一个 CS 会话里把同一
 * 16 位 RGB565 色值连续发 240*240 次。绕过 LVGL，用于自证 SPI+屏初始化打通。 */
void ST7789_FillColor(uint16_t color){
    uint8_t col[4] = {0, 0, 0, (uint8_t)(ST7789_w - 1)};
    uint8_t row[4] = {0, 0, 0, (uint8_t)(ST7789_H - 1)};
    write_cmd(ST7789_CMD_CASET); write_data(col, 4);
    write_cmd(ST7789_CMD_RASET); write_data(row, 4);
    write_cmd(ST7789_CMD_RAMWR);

    cs_low(); dc_data();
    uint8_t buf[256 * 2];                 /* 256 像素缓冲，减少 SPI 调用次数 */
    uint8_t hi = (uint8_t)(color >> 8), lo = (uint8_t)(color & 0xFF);
    for (int i = 0; i < 256; i++) { buf[2 * i] = hi; buf[2 * i + 1] = lo; }
    uint32_t remain = (uint32_t)ST7789_w * ST7789_H;
    while (remain > 0) {
        uint16_t chunk = (remain > 256U) ? 256 : (uint16_t)remain;
        HAL_SPI_Transmit(g_spi, buf, (uint16_t)(chunk * 2), HAL_MAX_DELAY);
        remain -= chunk;
    }
    cs_high();
}