#include "bgm_service.h"

//sequence，防止帧重复
static uint8_t g_seq=0;
//上位机播放状态
static bgm_state_t g_state=BGM_IDLE;
typedef void (*g_end_cb_t)(uint16_t);
g_end_cb_t g_end_cb=NULL;
typedef void (*g_switch_cb_t)(uint16_t);
g_switch_cb_t g_switch_cb=NULL;

void bgm_init(void) { g_seq = 0; g_state = BGM_IDLE; }

int bgm_play(uint16_t track) {
    uint8_t pay[2];
    PROTO_PutU16(pay, track);
    int r = PROTO_Send(CMD_PLAY_IDX, g_seq++, pay, 2);
    if (r == 0) g_state = BGM_PLAYING;
    return r;
}

int bgm_stop(void) {
    int r = PROTO_Send(CMD_STOP, g_seq++, NULL, 0);
    if (r == 0) g_state = BGM_IDLE;
    return r;
}

int bgm_pause(void) {
    int r = PROTO_Send(CMD_PAUSE, g_seq++, NULL, 0);
    if (r == 0) g_state = BGM_PAUSED;
    return r;
}

int bgm_resume(void) {
    int r = PROTO_Send(CMD_RESUME, g_seq++, NULL, 0);
    if (r == 0) g_state = BGM_PLAYING;
    return r;
}

int bgm_volume(uint8_t v) {
    uint8_t pay[2];
    pay[0] = v;
    pay[1] = v;
    return PROTO_Send(CMD_SET_VOL, g_seq++, pay, 2);
}

int bgm_set_mode(uint8_t mode) {
    return PROTO_Send(CMD_SET_MODE, g_seq++, &mode, 1);
}

void bgm_register_switch_cb(void (*cb)(uint16_t)) { g_switch_cb = cb; }

void bgm_register_end_cb(void (*cb)(uint16_t)) { g_end_cb = cb; }

/* 协议口收到 CMD_EVENT 时调用，分流两类事件：
 *   EVT_TRACK_END  -> g_end_cb（仅手动模式曲终触发，带“刚播完的序号”）
 *   EVT_AUTO_NEXT  -> g_switch_cb（连播/单曲循环自动切歌触发，带“新序号”）
 * 事件帧固定 3 字节（EVT_OFF_TYPE=0 u8, EVT_OFF_NEWIDX=1 u16），序号都在 pay[1..2]。 */
void bgm_on_event(uint8_t evt, const uint8_t *pay, uint16_t len) {
    uint16_t idx;
    if (len < EVT_PAY_LEN) return;                 /* 长度不足直接丢，避免越界读 */
    idx = PROTO_GetU16(pay + EVT_OFF_NEWIDX);
    if (evt == EVT_TRACK_END && g_end_cb != NULL) {
        g_end_cb(idx);
    } else if (evt == EVT_AUTO_NEXT && g_switch_cb != NULL) {
        g_switch_cb(idx);                          /* 自动切歌：通知 App 把高亮跳到新曲 */
    }
}

bgm_state_t bgm_state(void) { return g_state; }
