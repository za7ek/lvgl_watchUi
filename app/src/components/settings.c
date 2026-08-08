#include "settings.h"
#include "watchface.h"
#include <lvgl.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(settings, LOG_LEVEL_INF);

/* -------------------------------------------------------------------------
 * 布局常量
 * 面板 200×176px 居中，4 个按钮纵向排列。圆屏 SAFE_R=117，面板四角
 * 离圆心 hypot(100, 88)=133 > 117，但面板有半透明背景，四角溢出圆外
 * 没有问题——只是显示不出来，不会崩溃。
 * ---------------------------------------------------------------------- */
#define PANEL_W        200
#define PANEL_H        176
#define BTN_W          (PANEL_W - 24)   /* 176px，左右各留 12px padding */
#define BTN_H          34
#define BTN_GAP        6
#define BTN_START_Y    10               /* 第一个按钮距面板顶部 */
#define AUTO_CLOSE_MS  15000            /* 15 秒无操作自动关闭 */

/* 颜色 */
#define COL_PANEL_BG   0x1c1c1c
#define COL_PANEL_BOR  0x444444
#define COL_BTN_BG     0x2e2e2e
#define COL_BTN_BOR    0x555555
#define COL_BTN_TEXT   0xeeeeee
#define COL_BTN_PRESS  0x484848

static lv_obj_t   *s_panel;
static lv_timer_t *s_timer;

/* -------------------------------------------------------------------------
 * 内部工具
 * ---------------------------------------------------------------------- */
static void reset_timer(void)
{
    if (s_timer) {
        lv_timer_reset(s_timer);
    }
}

/* -------------------------------------------------------------------------
 * 按钮回调：每次点击调对应的切换函数，并重置自动关闭计时器
 * ---------------------------------------------------------------------- */
static void btn_lang_cb(lv_event_t *e)
{
    ARG_UNUSED(e);
    watchface_switch_language();
    reset_timer();
}

static void btn_theme_cb(lv_event_t *e)
{
    ARG_UNUSED(e);
    watchface_switch_theme();
    reset_timer();
}

static void btn_moon_cb(lv_event_t *e)
{
    ARG_UNUSED(e);
    watchface_switch_moon_display();
    reset_timer();
}

static void btn_batt_cb(lv_event_t *e)
{
    ARG_UNUSED(e);
    watchface_switch_battery_display();
    reset_timer();
}

/* -------------------------------------------------------------------------
 * 自动关闭定时器回调
 * ---------------------------------------------------------------------- */
static void auto_close_cb(lv_timer_t *t)
{
    ARG_UNUSED(t);
    settings_hide();
}

/* -------------------------------------------------------------------------
 * 创建一个按钮行（带文字标签）并挂到 parent 上
 * ---------------------------------------------------------------------- */
static lv_obj_t *make_btn(lv_obj_t *parent, const char *text,
                           lv_event_cb_t cb, int y_ofs)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, BTN_W, BTN_H);
    lv_obj_set_pos(btn, (PANEL_W - BTN_W) / 2, y_ofs);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COL_BTN_BG),   LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn,   LV_OPA_COVER,                LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(COL_BTN_BOR), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1,                        LV_PART_MAIN);
    lv_obj_set_style_radius(btn,   6,                            LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn,  0,                            LV_PART_MAIN);
    /* 按下时高亮 */
    lv_obj_set_style_bg_color(btn, lv_color_hex(COL_BTN_PRESS), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_BTN_TEXT), LV_PART_MAIN);
    lv_obj_center(lbl);

    return btn;
}

/* -------------------------------------------------------------------------
 * 公开 API
 * ---------------------------------------------------------------------- */
void settings_init(void)
{
    /* 面板容器。关闭面板的点击逻辑由 watchface.c 的透明输入层处理。 */
    s_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_panel, PANEL_W, PANEL_H);
    lv_obj_align(s_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(COL_PANEL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_panel,   LV_OPA_90,                   LV_PART_MAIN);
    lv_obj_set_style_border_color(s_panel, lv_color_hex(COL_PANEL_BOR), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_panel, 1,                        LV_PART_MAIN);
    lv_obj_set_style_radius(s_panel,   12,                           LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_panel,  0,                            LV_PART_MAIN);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 4 个设置按钮（纯英文标签，避免汉字用到表盘 CJK 字体之外的字形） */
    int y = BTN_START_Y;
    make_btn(s_panel, "Language",       btn_lang_cb,  y); y += BTN_H + BTN_GAP;
    make_btn(s_panel, "Theme",          btn_theme_cb, y); y += BTN_H + BTN_GAP;
    make_btn(s_panel, "Moon display",   btn_moon_cb,  y); y += BTN_H + BTN_GAP;
    make_btn(s_panel, "Battery",        btn_batt_cb,  y);

    /* 自动关闭定时器（初始暂停，show 时启动） */
    s_timer = lv_timer_create(auto_close_cb, AUTO_CLOSE_MS, NULL);
    lv_timer_pause(s_timer);

    settings_hide();

    LOG_INF("settings panel initialized");
}

void settings_show(void)
{
    if (!s_panel) {
        return;
    }
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_panel);
    lv_timer_reset(s_timer);
    lv_timer_resume(s_timer);
    LOG_INF("settings panel shown");
}

void settings_hide(void)
{
    if (!s_panel) {
        return;
    }
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(s_timer);
    LOG_INF("settings panel hidden");
}

bool settings_is_visible(void)
{
    if (!s_panel) {
        return false;
    }
    return !lv_obj_has_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
}
