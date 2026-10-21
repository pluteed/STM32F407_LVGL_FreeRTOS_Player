#ifndef BGM_SERVICE_H
#define BGM_SERVICE_H

#include <stdint.h>
#include "proto.h"


#define BGM_MODE_MANUAL 0u
#define BGM_MODE_ONE    1u
#define BGM_MODE_SEQ    2u

typedef enum {
    BGM_IDLE = 0,
    BGM_PLAYING,
    BGM_PAUSED
} bgm_state_t;

void  bgm_init(void);                          /* 初始化序号/状态 */
int    bgm_play(uint16_t track);               /* -> PROTO_Send(CMD_PLAY_IDX) */
int    bgm_stop(void);                         /* -> CMD_STOP */
int    bgm_pause(void);                        /* -> CMD_PAUSE */
int    bgm_resume(void);                       /* -> CMD_RESUME */
int    bgm_volume(uint8_t v);                  /* -> CMD_SET_VOL（左右相同） */
void   bgm_register_end_cb(void (*cb)(uint16_t ended_idx)); /* 一曲播完回调 */
void   bgm_register_switch_cb(void (*cb)(uint16_t new_idx)); /* 自动切歌回调 */
int    bgm_set_mode(uint8_t mode);            /* 切换 手动/单曲/顺序 三种播放模式 */
void   bgm_on_event(uint8_t evt, const uint8_t *pay, uint16_t len); /* 协议口分发时调用 */
bgm_state_t bgm_state(void);

#endif