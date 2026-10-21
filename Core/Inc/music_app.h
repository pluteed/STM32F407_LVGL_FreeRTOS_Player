/*==============================================================================
 * music_app.h —— 音乐播放器 App 头文件（歌单缓冲访问接口）
 *
 * 全局歌单缓冲（g_songlist[]）放在 music_app.c 的 .bss 静态区；
 * 开机由 music_list_bootload() 触发一次读取填满。任何 App 都能通过下面
 * 两个访问器拿歌名 / 总数（例如做“正在播放”标题），不依赖 Music App 是否打开。
 *============================================================================*/
#ifndef MUSIC_APP_H
#define MUSIC_APP_H

#include <stdint.h>

void music_create(void);
void music_destroy(void);
void music_tick(void);

void         music_list_bootload(void);   /* 开机读列表进全局缓冲（host_main 在 gui_task 启动后调一次） */
uint16_t     music_list_count(void);      /* 缓冲里有效曲目数（0 = 还没读完 / 无卡） */
const char  *music_list_name(uint16_t idx);  /* 第 idx 首歌名，越界返回 NULL */

#endif /* MUSIC_APP_H */
