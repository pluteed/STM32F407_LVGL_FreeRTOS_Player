/*==============================================================================
 * zork_app.c —— 迷你场景漫游（哑巴主角版），你选的演示 App
 *
 * 设计要点：
 *   1) 主角是个“哑巴”——不能说话，所以【没有软键盘、没有命令解析、没有任何文字输入】。
 *      操作方式：上/下/左/右方向键让主角在场景里走动；走到场景边缘且那个方向有路，
 *      就“走入”相邻场景（主角的位置从对侧边缘进入新场景）。
 *   2) 本 App【完全不碰 BGM】（不调 bgm_play / bgm_stop）。背景音乐由独立的
 *      bgm_service 常驻播放——你在 Music App 点了一首歌，切到 Zork 照样播。
 *      这正是要演示的：“运行 App 的同时音乐照常播放”，播放能力彻底与应用解耦。
 *
 * 输入：方向键经 keypad group 的 LV_EVENT_KEY 事件送达（见 zork_key_cb）。
 * 编译：C90（ARM Compiler 5 默认）。变量声明在块首，for 内不声明。
 *============================================================================*/
#include "lvgl.h"
#include "app_framework.h"
#include "indev_port.h"   /* 5 向键导航的 keypad group | Group | 控件焦点组 */

/* 场景图：exits 顺序固定为 [N, E, S, W]，值 = 相邻场景序号，-1 表示无路 */
typedef struct {
    const char *name;     /* 场景名（标题） */
    const char *desc;     /* 一行描述 */
    lv_color_t  bg;       /* 场景背景色 */
    int8_t      exits[4]; /* N,E,S,W */
} scene_t;

static const scene_t g_scenes[] = {
    { "镇口", "镇子入口，北面是森林，南边是湖。\r\n",
      LV_COLOR_MAKE(0x37, 0x6A, 0xB7), { 1, -1,  3, -1 } },
    { "森林", "幽暗的树林，东边有个山洞。\r\n",
      LV_COLOR_MAKE(0x2E, 0x7D, 0x32), { -1, 2,  0, -1 } },
    { "山洞", "潮湿的洞窟，西边通回森林。\r\n",
      LV_COLOR_MAKE(0x6D, 0x4C, 0x41), { -1, -1, -1,  1 } },
    { "湖边", "波光粼粼，北面回到镇口。\r\n",
      LV_COLOR_MAKE(0x00, 0x8A, 0x9E), { 0, -1, -1, -1 } },
};
static uint8_t g_scene = 0;   /* 当前场景序号 */

/* 主角在“可走区域”内的位置（像素，相对屏幕） */
#define PLAY_X      12
#define PLAY_Y      46
#define PLAY_W      216
#define PLAY_H      250
#define HERO_R      12      /* 主角半身半径（头+身合为一个圆角块） */
#define STEP        10      /* 每按一次方向键走的距离 */

static lv_obj_t *g_title  = NULL;   /* 场景标题 */
static lv_obj_t *g_desc   = NULL;   /* 场景描述 */
static lv_obj_t *g_hero   = NULL;   /* 主角（头+身） */
static lv_obj_t *g_root   = NULL;   /* 整屏根对象，用于 group 焦点 + 收方向键 */
static int16_t   g_px = 0, g_py = 0;

/* 把主角放到 (x,y)（相对屏幕） */
static void hero_place(int16_t x, int16_t y) {
    lv_obj_set_pos(g_hero, x - HERO_R, y - HERO_R * 2);
}

/* 画/刷新当前场景（背景色、标题、描述），主角归位到区域中心 */
static void draw_scene(void) {
    const scene_t *s = &g_scenes[g_scene];
    lv_obj_set_style_bg_color(lv_scr_act(), s->bg, 0);
    lv_label_set_text(g_title, s->name);
    lv_label_set_text(g_desc, s->desc);
    g_px = (int16_t)(PLAY_X + PLAY_W / 2);
    g_py = (int16_t)(PLAY_Y + PLAY_H / 2);
    hero_place(g_px, g_py);
}

/* 方向键移动：走到边缘且有路则换场景，无路则卡在边界 */
static void hero_move(lv_key_t key) {
    uint8_t dir;                 /* 0=N 1=E 2=S 3=W，与 exits[] 对应 */
    int16_t nx = g_px, ny = g_py;
    if      (key == LV_KEY_UP)    { dir = 0; ny -= STEP; }
    else if (key == LV_KEY_DOWN)  { dir = 2; ny += STEP; }
    else if (key == LV_KEY_LEFT)  { dir = 3; nx -= STEP; }
    else if (key == LV_KEY_RIGHT) { dir = 1; nx += STEP; }
    else return;

    int16_t lo_x = (int16_t)(PLAY_X + HERO_R);
    int16_t hi_x = (int16_t)(PLAY_X + PLAY_W - HERO_R);
    int16_t lo_y = (int16_t)(PLAY_Y + HERO_R);
    int16_t hi_y = (int16_t)(PLAY_Y + PLAY_H - HERO_R);

    if (nx < lo_x) {
        if (g_scenes[g_scene].exits[3] >= 0) { g_scene = (uint8_t)g_scenes[g_scene].exits[3]; draw_scene(); return; }
        nx = lo_x;
    } else if (nx > hi_x) {
        if (g_scenes[g_scene].exits[1] >= 0) { g_scene = (uint8_t)g_scenes[g_scene].exits[1]; draw_scene(); return; }
        nx = hi_x;
    }
    if (ny < lo_y) {
        if (g_scenes[g_scene].exits[0] >= 0) { g_scene = (uint8_t)g_scenes[g_scene].exits[0]; draw_scene(); return; }
        ny = lo_y;
    } else if (ny > hi_y) {
        if (g_scenes[g_scene].exits[2] >= 0) { g_scene = (uint8_t)g_scenes[g_scene].exits[2]; draw_scene(); return; }
        ny = hi_y;
    }
    g_px = nx; g_py = ny;
    hero_place(g_px, g_py);
}

/* 方向键事件（焦点在 g_root 时，keypad group 把 LV_KEY_* 发到这里） */
static void zork_key_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    hero_move((lv_key_t)lv_event_get_key(e));
}

/* 造主角：头(圆) + 身(圆角块)，合成一个可整体移动的 obj */
static void hero_create(lv_obj_t *parent) {
    g_hero = lv_obj_create(parent);
    lv_obj_set_size(g_hero, HERO_R * 2, HERO_R * 4);
    lv_obj_set_style_bg_opa(g_hero, LV_OPA_COVER, 0);   /* v9 无 lv_obj_clear_style_prop；且本行已设满不透明，clear 行多余，删之 */
    lv_obj_set_style_bg_color(g_hero, lv_color_white(), 0);
    lv_obj_set_style_radius(g_hero, HERO_R, 0);
    lv_obj_clear_flag(g_hero, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *head = lv_obj_create(g_hero);
    lv_obj_set_size(head, HERO_R * 2, HERO_R * 2);
    lv_obj_align(head, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(head, lv_color_white(), 0);
    lv_obj_set_style_radius(head, HERO_R, 0);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_CLICKABLE);
}

void zork_create(void) {
    lv_obj_t *scr = lv_scr_act();
    lv_group_t *grp = indev_port_get_group();

    g_root = lv_obj_create(scr);
    lv_obj_set_size(g_root, 240, 320);
    lv_obj_align(g_root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(g_root, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_CLICKABLE);

    g_title = lv_label_create(g_root);
    lv_obj_set_style_text_font(g_title, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(g_title, lv_color_white(), 0);
    lv_label_set_text(g_title, "镇口");
    lv_obj_align(g_title, LV_ALIGN_TOP_MID, 0, 6);

    g_desc = lv_label_create(g_root);
    lv_obj_set_style_text_color(g_desc, lv_color_white(), 0);
    lv_label_set_text(g_desc, "");
    lv_obj_align(g_desc, LV_ALIGN_TOP_MID, 0, 30);   /* 下移：避开了左上角框架 Home 键所在的顶部条(y 4~28) */

    hero_create(g_root);

    /* 底部提示：点明“哑巴、方向键、音乐独立播放”的演示意图 */
    lv_obj_t *hint = lv_label_create(g_root);
    lv_obj_set_style_text_color(hint, lv_color_white(), 0);
                lv_label_set_text(hint, "Arrow keys to move. Reach an edge to enter the next room (hero is mute; BGM plays independently).");
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);

    g_scene = 0;
    draw_scene();

    /* 收方向键：把根对象加入 keypad group 并设为焦点 */
    if (grp != NULL) {
        lv_group_add_obj(grp, g_root);
        lv_group_focus_obj(g_root);
        lv_obj_add_event_cb(g_root, zork_key_cb, LV_EVENT_KEY, NULL);
    }
}

void zork_destroy(void) {
    /* 不碰 BGM：切走后 Music App 点播的歌继续播。 */
    g_root = NULL; g_title = NULL; g_desc = NULL; g_hero = NULL;
}

void zork_tick(void) {
    /* 场景漫游纯事件驱动（方向键），这里留空。 */
}
