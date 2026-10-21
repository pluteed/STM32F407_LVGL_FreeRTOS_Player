#include "app_framework.h"
#include "indev_port.h"

LV_IMG_DECLARE(music_icon);
LV_IMG_DECLARE(zork_icon);



extern void music_create(void);
extern void music_destroy(void);
extern void music_tick(void);
extern void zork_create(void);
extern void zork_destroy(void);
extern void zork_tick(void);

static const app_entry_t g_apps[] = {
    { "Music", &music_icon, music_create, music_destroy, music_tick },
    { "Zork",  &zork_icon, zork_create,  zork_destroy,  zork_tick  },
};

static uint8_t g_active=0xFF;   /* 0xFF = 在 Launcher，无活跃 App（哨兵值） */
typedef void (*rx_cb_t)(const void* frame);
static rx_cb_t g_app_rx=NULL;
void app_set_rx_handler(rx_cb_t cb){g_app_rx=cb;}
void app_dispatch_rx(const void *frame){if(g_app_rx)g_app_rx(frame);}

/* ---- 实体 Home/ESC 键拦截钩子（可选功能）----------------------------------
 * 遍历输入设备找 keypad，挂一个 LV_EVENT_KEY 钩子：
 * 只要有键上报 LV_KEY_HOME 或 LV_KEY_ESC，且当前确实在某个 App 内，就返回 Launcher。
 * 用 lv_indev_get_next 遍历，不需要 indev_port 额外暴露接口，改动面最小。 */
static lv_indev_t * find_keypad_indev(void){
    lv_indev_t *indev=lv_indev_get_next(NULL);
    while(indev){
        if(lv_indev_get_type(indev)==LV_INDEV_TYPE_KEYPAD) return indev;
        indev=lv_indev_get_next(indev);
    }
    return NULL;
}

static void framework_key_cb(lv_event_t *e){
    lv_indev_t *indev=(lv_indev_t *)lv_event_get_target(e);
    uint32_t key;
    if(!indev) return;
    if(lv_indev_get_state(indev)!=LV_INDEV_STATE_PRESSED) return;  /* 只在按下沿响应一次 */
    key=lv_indev_get_key(indev);
    if(key!=LV_KEY_HOME && key!=LV_KEY_ESC) return;
    if(g_active>=app_count()) return;   /* 已经在 Launcher，无需返回 */
    lv_indev_wait_release(indev);      /* 吃掉这一键，别再往下传给当前聚焦控件 */
    app_go_home();
}

/* 从任意 App 返回 Launcher（方案 A）：拆当前 App、清屏、重跑一次启动器页面。
   注：app_go_home 在 app_framework.h 里已对外声明（外部链接），此处不能再写 static，
       否则“static 声明跟在非 static 声明之后”会编译报错。 */

/* 框架级 Home 返回键回调（触屏点击触发，不进 keypad group，避免干扰方向键语义） */
static void home_btn_cb(lv_event_t *e){
    (void)e;
    app_go_home();
}

uint8_t app_count(void){return (uint8_t)(sizeof(g_apps)/sizeof(g_apps[0]));}
uint8_t app_active_idx(void){return (uint8_t)g_active;}

//所有app图标共用一个cb函数，通过usrdata的不同来区分启动哪个app
static void launcher_btn_cb(lv_event_t *e){
    uint8_t idx=(uint8_t)lv_event_get_user_data(e);
    app_launch(idx);
}

void app_framework_init(void){
    lv_obj_t *scr=lv_scr_act();
    lv_group_t *grp=indev_port_get_group();
    static uint8_t s_key_hooked=0;   /* 实体 Home 钩子只挂一次（每次回主页都会重跑本函数） */

    /* 挂实体 Home/ESC 键拦截钩子；首次进来时 keypad 可能还没建，找不到就下次再试 */
    if(!s_key_hooked){
        lv_indev_t *kb=find_keypad_indev();
        if(kb){lv_indev_add_event_cb(kb,framework_key_cb,LV_EVENT_KEY,NULL);s_key_hooked=1;}
    }

    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);  /* 进 Launcher 先复位背景（防上一 App 改过的场景色残留） */

    const int16_t BTN_W = 100, BTN_H = 100, GAP = 16;
    const int16_t STEP  = BTN_H + GAP;         
    uint8_t n = app_count();
    uint8_t i;
    for(i=0;i<n;i++){
        lv_obj_t *btn;
        int16_t yoff;                      
        btn = lv_btn_create(scr);
        lv_obj_set_size(btn, BTN_W, BTN_H);
        /* keypad 模式下方向键移动的是“焦点(focus：当前被选控件)”，给选中按钮加蓝边+浅蓝底，
         * 否则白底白按钮上看不出焦点在哪 → 误以为“按键没反应”。首个按钮加进 group 时自动获焦。 */
        lv_obj_set_style_border_color(btn, lv_color_make(0x10,0x40,0xFF), LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(btn, 3, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(btn, lv_color_make(0xE0,0xE8,0xFF), LV_PART_MAIN | LV_STATE_FOCUSED);
        yoff = (int16_t)(i * 2 - (n - 1)) * (STEP / 2);
        lv_obj_align(btn, LV_ALIGN_CENTER, 0, yoff);

        /* 图标已重生成成 LV_COLOR_FORMAT_RGB565（SW renderer 原生支持，不再崩屏）；
         * 透明像素填白底融入白色按钮背景。 */
        if(g_apps[i].icon){
            lv_obj_t *img=lv_img_create(btn);
            lv_img_set_src(img,g_apps[i].icon);
            lv_obj_align(img,LV_ALIGN_CENTER,0,-14);
            lv_obj_t *lab = lv_label_create(btn);
            lv_label_set_text(lab, g_apps[i].name);
            lv_obj_align(lab, LV_ALIGN_BOTTOM_MID, 0, -6);
        }

        lv_obj_add_event_cb(btn,launcher_btn_cb,LV_EVENT_CLICKED,(void*)i);
        if(grp) lv_group_add_obj(grp,btn);   /* 补：加进 keypad group，5 向键才选得中按钮（原来 grp 取了没用） */
    }
}

void app_launch(uint8_t idx) {
    if (idx >= app_count()) return;

    /* 仅当当前确有活跃 App（非 Launcher 哨兵 0xFF）才先拆它；
       顺便修掉“首启 idx==0 误调未创建的 Music destroy()”隐患 */
    if (g_active < app_count() && g_apps[g_active].destroy) g_apps[g_active].destroy();
    app_set_rx_handler(NULL);
    lv_obj_clean(lv_scr_act());

    g_active = idx;
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);  /* 背景复位默认浅色，App 要改色自行设 */
    if (g_apps[idx].create) g_apps[idx].create();

    /* 框架自动在每个 App 左上角放一个 Home 键：所有 App 通用，无需各 App 自行加。
       触屏点击返回 Launcher；故意不进 keypad group，以免干扰 Zork 方向键语义。 */
    {
        lv_obj_t *home = lv_btn_create(lv_scr_act());
        lv_obj_set_size(home, 48, 24);
        lv_obj_align(home, LV_ALIGN_TOP_LEFT, 4, 4);
        lv_obj_t *hl = lv_label_create(home);
        lv_label_set_text(hl, "Home");
        lv_obj_center(hl);
        lv_obj_add_event_cb(home, home_btn_cb, LV_EVENT_CLICKED, NULL);
    }
}

/* 返回 Launcher（方案 A）：拆当前 App，清屏，重跑一次启动器页面 */
void app_go_home(void) {
    if (g_active < app_count() && g_apps[g_active].destroy) g_apps[g_active].destroy();
    app_set_rx_handler(NULL);
    lv_obj_clean(lv_scr_act());
    app_framework_init();      /* 重建 Launcher（标题 + 各 App 按钮），内部已复位背景 */
    g_active = 0xFF;           /* 回到 Launcher 哨兵态 */
}

void app_tick_active(void) {
    /* Launcher 态 g_active==0xFF 时不做任何 App 的 tick，避免越界访问 g_apps[] */
    if (g_active < app_count() && g_apps[g_active].tick) g_apps[g_active].tick();
}
