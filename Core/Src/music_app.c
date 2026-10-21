/*==============================================================================
 * music_app.c —— 音乐播放器（全局缓冲版）
 *
 * 改动要点（相对原骨架）：
 *   1) 歌单存进全局缓冲 g_songlist[]（.bss 静态区，256 首 ≈ 17 KB，F407 绰绰有余）。
 *   2) 开机由 music_list_bootload() 先等下位机第一帧 STATUS，再发一次 CMD_LIST_REQ，
 *      列表帧（BEGIN/ITEM/END）
 *      全部进缓冲；Music App 打开时直接拿缓冲渲染 lv_list，不再每次重读。
 *      （按你的要求：不处理“运行中途换卡”，下次开机重新读即可。）
 *   3) 收帧回调 music_rx 只运行在【协议任务上下文】，只写全局缓冲，绝不调用 lv_*
 *      （LVGL | Light and Versatile Graphics Library | 轻量级图形库 线程安全铁律）；
 *      lv_list 的绘制交给 music_tick（gui_task 上下文，持 g_lvgl_mutex | Mutual
 *      Exclusion | 互斥量）增量刷新，规避跨任务踩 LVGL 的竞态。
 *   4) 列表项点击即播（补全原 TODO：lv_list_add_btn 绑定 idx）。
 *
 * 编译：C90（ARM Compiler 5 默认）。变量声明在块首，for 内不声明。
 *============================================================================*/
#include "lvgl.h"
#include "app_framework.h"
#include "bgm_service.h"
#include "proto.h"
#include "music_app.h"
#include "indev_port.h"   /* 提供 indev_port_get_group() 的声明，否则被当隐式 int 返回 */
#include "cmsis_os.h"    //osDelay

/* ---- 全局歌单缓冲（静态区 .bss | Block Started by Symbol | 未初始化全局/静态变量段）---- */
#define MUSIC_LIST_MAX  256u     /* 最多缓存曲目数（按 SD 卡常见规模取，可改） */
#define MUSIC_NAME_MAX  64u      /* 单首歌名缓冲（GBK | Guojia Biaozhun Kuozhan | 国标扩展，含 '\0'） */

typedef struct {
    uint16_t index;                  /* 曲目序号（与下位机一致） */
    char     name[MUSIC_NAME_MAX];   /* GBK 歌名，'\0' 结尾（区别于协议帧“不定长不补\0”） */
} song_entry_t;

static song_entry_t g_songlist[MUSIC_LIST_MAX];
static uint16_t g_song_count   = 0;   /* 有效曲目数（= 最大序号 + 1） */
static uint8_t  g_list_ready   = 0;   /* 0=未读完 1=已收 END 且总数吻合 2=读完但总数不符（可能丢帧） */
static uint8_t  g_list_rendered = 0;  /* 已画进 lv_list 的条目数（增量渲染游标） */

//下位机就绪标志：收到第一帧 STATUS 时置 1
//RCT6 的 STATUS 只在 App_Poll 里发，而 App_Poll 在 App_Init 全部跑完之后才开始
//proto_rx_task 里写、gui_task 里读
static volatile uint8_t g_slave_ready = 0;

static lv_obj_t *g_list = NULL;
static int16_t   g_sel  = -1;         /* 当前选中/播放曲目序号，-1 表示未选 */

/* 播放高亮相关（自动连播时让界面跟着跳） */
static lv_obj_t *g_list_btns[MUSIC_LIST_MAX] = { NULL };  /* 列表项按钮指针，按序号索引 */
static lv_obj_t *g_play_btn  = NULL;   /* 当前高亮（播放中）的按钮 */
static uint8_t  g_mode      = BGM_MODE_MANUAL;  /* 当前播放模式，默认手动（与下位机默认一致） */
static lv_obj_t *g_mode_label = NULL; /* 右上角模式按钮上的文字 */

/* 前向声明：列表点击回调 list_item_cb 会用到 set_playing（C90 要求调用前可见） */
static void set_playing(uint16_t idx);

/* 协议帧到达（proto_rx_task -> app_dispatch_rx -> 这里）。运行在【协议任务上下文】，
 * 只写全局缓冲，绝不调用 lv_*（线程安全）。帧来源始终是 proto.c 的 g_rx_frame。 */
static void music_rx(const void *frame) {
    const proto_frame_t *f = (const proto_frame_t *)frame;
    if (f->cmd == CMD_LIST_BEGIN) {
        g_song_count = 0;
        g_list_ready = 0;
        /* g_list_rendered 不在此复位：它由 music_create 在（重建空的）lv_list 时归零，
         * 避免“运行中途重读”把条目重复画进同一个 lv_list 控件。按你的要求本设计不处理中途换卡。 */
    }
    else if (f->cmd == CMD_LIST_ITEM) {
        uint16_t idx = PROTO_GetU16(f->pay);            /* 序号在 pay[0..1] */
        uint16_t n   = (f->len > 2) ? (uint16_t)(f->len - 2) : 0;  /* 歌名长度 */
        if (idx < MUSIC_LIST_MAX) {
            song_entry_t *e = &g_songlist[idx];
            uint16_t i;
            e->index = idx;
            if (n > (MUSIC_NAME_MAX - 1)) n = MUSIC_NAME_MAX - 1;
            for (i = 0; i < n; i++) e->name[i] = (char)f->pay[2 + i];  /* GBK，无 '\0' */
            e->name[n] = 0;
            if (idx + 1 > g_song_count) g_song_count = (uint16_t)(idx + 1);
        }
    }
    else if (f->cmd == CMD_LIST_END) {
        uint16_t total = PROTO_GetU16(f->pay);          /* END 载荷 = u16 总曲目数 */
        g_list_ready = (total == g_song_count) ? 1u : 2u;  /* 2 = 总数不符，可能中途丢帧 */
    }
    else if (f->cmd == CMD_STATUS) {
        g_slave_ready = 1;   //收到心跳，下位机初始化完成
    }
    /* 其它帧本 App 暂不处理（沿用原 TODO） */
}

/* 列表项被点击：记下序号并立即播放（补全原 TODO） */
static void list_item_cb(lv_event_t *e) {
    uint16_t idx = (uint16_t)(uint32_t)lv_event_get_user_data(e);
    uint8_t p[2];
    PROTO_PutU16(p, idx);
    PROTO_Send(CMD_PLAY_IDX, 0, p, 2);
    set_playing(idx);   /* 立即高亮（下位机真正播起后会回 STATUS / AUTO_NEXT） */
}

static void btn_cb(lv_event_t *e) {
    uint32_t act = (uint32_t)lv_event_get_user_data(e);
    if (act == 0) {                                       /* Play：用选中序号 */
        uint8_t p[2];
        PROTO_PutU16(p, (uint16_t)(g_sel < 0 ? 0 : g_sel));
        PROTO_Send(CMD_PLAY_IDX, 0, p, 2);
    }
    else if (act == 1) PROTO_Send(CMD_PAUSE, 0, NULL, 0);
    else if (act == 2) PROTO_Send(CMD_NEXT,  0, NULL, 0);
    else if (act == 3) PROTO_Send(CMD_PREV,  0, NULL, 0);
}

/* ---- 播放模式切换（Music App 右上角按钮）---- */
static const char *mode_name(uint8_t m) {
    static const char *names[3] = { "手动", "单曲", "顺序" };
    if (m > 2) return "未知";
    return names[m];
}

/* 把“播放中”高亮移到 idx 对应列表项（仅在 gui_task 调用，线程安全） */
static void set_playing(uint16_t idx) {
    if (g_play_btn != NULL) {
        lv_obj_remove_state(g_play_btn, LV_STATE_CHECKED);
        g_play_btn = NULL;
    }
    g_sel = (int16_t)idx;
    if (g_list != NULL && idx < MUSIC_LIST_MAX && g_list_btns[idx] != NULL) {
        g_play_btn = g_list_btns[idx];
        lv_obj_add_state(g_play_btn, LV_STATE_CHECKED);   /* CHECKED 态带底色 = 当前播放 */
    }
}

/* 模式按钮：循环 手动→单曲→顺序，下发 CMD_SET_MODE，刷新文字 */
static void mode_btn_cb(lv_event_t *e) {
    (void)e;
    g_mode = (uint8_t)((g_mode + 1u) % 3u);
    bgm_set_mode(g_mode);
    if (g_mode_label != NULL) lv_label_set_text(g_mode_label, mode_name(g_mode));
}

/* 下位机自动切歌（EVT_AUTO_NEXT）回调：高亮跳到新曲（App 已退出则忽略） */
static void on_auto_next(uint16_t new_idx) {
    if (g_list == NULL) return;
    set_playing(new_idx);
}

//开机读列表：挂回调 -> 等下位机就绪 -> 要列表，结果进 g_songlist[]
void music_list_bootload(void) {
    uint16_t waited;

    //先挂回调，否则收不到下位机的 STATUS 心跳
    app_set_rx_handler(music_rx);

    //等第一帧 STATUS，RCT6 每秒上报一次，上限 3 秒
    for (waited = 0; waited < 60u; waited++) {
        if (g_slave_ready) break;
        osDelay(50);          //50 tick x 60 = 3000ms
    }

    PROTO_Send(CMD_LIST_REQ, 0, NULL, 0);
}

void music_create(void) {
    lv_obj_t *scr = lv_scr_act();
    lv_group_t *grp = indev_port_get_group();   /* 修：原脚手架漏了这行，导致 grp 未声明编译不过 */
    uint16_t k;

    /* 跨切换重置高亮状态（g_list_btns 里可能残留上一轮指向已释放对象的悬空指针） */
    g_play_btn = NULL;
    for (k = 0; k < MUSIC_LIST_MAX; k++) g_list_btns[k] = NULL;

    g_list = lv_list_create(scr);
    lv_obj_set_size(g_list, 220, 150);
    lv_obj_align(g_list, LV_ALIGN_TOP_MID, 0, 32);   /* 下移 28px：给左上角框架 Home 键让出顶部条 */

    g_list_rendered = 0;
    app_set_rx_handler(music_rx);               /* 复用同一缓冲回调（开机已挂，这里幂等） */
    if (!g_list_ready) {
        PROTO_Send(CMD_LIST_REQ, 0, NULL, 0);    /* 缓冲还没读完就再要一次（开机读未完成兜底） */
    }

    /* 模式切换按钮：左上 Home，右上放模式；循环切 手动→单曲→顺序，下发 CMD_SET_MODE */
    {
        lv_obj_t *mb = lv_btn_create(scr);
        lv_obj_set_size(mb, 56, 24);
        lv_obj_align(mb, LV_ALIGN_TOP_RIGHT, -4, 4);
        g_mode_label = lv_label_create(mb);
        lv_label_set_text(g_mode_label, mode_name(g_mode));
        lv_obj_center(g_mode_label);
        lv_obj_add_event_cb(mb, mode_btn_cb, LV_EVENT_CLICKED, NULL);
        if (grp != NULL) lv_group_add_obj(grp, mb);
    }
    bgm_register_switch_cb(on_auto_next);       /* 注册自动切歌回调：连播时高亮跳到新曲 */

    static const char *labels[4] = { "Play", "Pause", "Next", "Prev" };
    uint8_t i;
    for (i = 0; i < 4; i++) {
        lv_obj_t *b = lv_btn_create(scr);
        lv_obj_set_size(b, 50, 30);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, (int16_t)((int16_t)i - 1) * 56, -6);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, labels[i]);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, btn_cb, LV_EVENT_CLICKED, (void *)(uint32_t)i);
        if (grp != NULL) lv_group_add_obj(grp, b);   /* Play/Pause/Next/Prev 可被 5 向键选中并确认 */
    }
}

void music_destroy(void) {
    app_set_rx_handler(NULL);
    g_list = NULL;
    g_list_rendered = 0;
    g_play_btn = NULL;        /* 高亮按钮指针失效（对象已被 lv_obj_clean 释放） */
    g_mode_label = NULL;
    /* 全局缓冲 g_songlist[] 保留（跨 App 切换不丢），下次打开直接渲染；
       g_list_btns[] 在 music_create 开头统一清零，避免残留悬空指针 */
}

void music_tick(void) {
    /* 增量把缓冲里“尚未画进 lv_list”的条目画出来；运行在 gui_task（持 g_lvgl_mutex），线程安全。
     * 开机读已完成时一次性画满；读的过程中打开则边到边画。 */
    lv_group_t *grp = indev_port_get_group();
    while (g_list_rendered < g_song_count && g_list != NULL) {
        const song_entry_t *e = &g_songlist[g_list_rendered];
        lv_obj_t *btn = lv_list_add_btn(g_list, NULL, e->name);
        g_list_btns[e->index] = btn;
        lv_obj_set_style_bg_color(btn, lv_color_make(0x2E, 0x7D, 0x32), LV_STATE_CHECKED);  /* 播放中高亮底色 */
        lv_obj_add_event_cb(btn, list_item_cb, LV_EVENT_CLICKED, (void *)(uint32_t)e->index);
        if (grp != NULL) lv_group_add_obj(grp, btn);   /* 列表项也可被 5 向键导航 */
        g_list_rendered++;
    }
}

uint16_t music_list_count(void) { return g_song_count; }
const char *music_list_name(uint16_t idx) {
    if (idx >= g_song_count) return NULL;
    return g_songlist[idx].name;
}
