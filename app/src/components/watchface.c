#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "watchface.h"
#include "segment34.h"
#include "icons.h"
#include "lunar_calendar.h"
#include "locale.h"
#include "theme.h"
#include "lv_font_cjk.h"
#include "lv_font_segments80.h"
#include "lv_font_led.h"
#include "lv_font_led_small.h"
#include "lv_font_xsmol.h"
#include "lv_font_moon.h"
#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <time.h>

LOG_MODULE_REGISTER(watchface, LOG_LEVEL_INF);

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

#define CLOCK_W 220
#define CLOCK_H 80
#define CLOCK_X 10
#define CLOCK_Y 60

/* 实体线字体（Montserrat系列）替代点阵LED字体
 * 第一行标签、字段标签：FONT_LABEL = montserrat_8（比时间行更小）
 * 第二行时间、日期行：FONT_DATA = montserrat_12 / FONT_TIME_SMALL = montserrat_10
 * 月相文字（更小）：FONT_MOON = montserrat_8
 */
#define FONT_LABEL      &lv_font_xsmol
#define FONT_DATA       &lv_font_montserrat_12
#define FONT_TIME_SMALL &lv_font_montserrat_10
#define FONT_MOON       &lv_font_montserrat_8
#define FONT_MOON_IMAGE &lv_font_moon   /* 月相图片字体，20×20px，8 个月相（'0'-'7'） */
#define FONT_MED        &lv_font_montserrat_12
#define FONT_CJK        &lv_font_cjk    /* CJK 中文字体，10px，4bpp，用于日期行/天气/农历 */
#define FONT_LED        &lv_font_led   /* 用于字段数值行 + 倒数第二行steps（LED点阵风格） */

#define LED_DIGIT_W 16    /* adv_w=16px (14px char + 2px gap) */
#define LED_DIGIT_H 20    /* 2x2 blocks, 1px gaps → 20px tall */
#define LED_FIELD_GAP 0   /* no extra gap; spacing is in adv_w */
#define FIELD1_DIGITS 4
#define FIELD2_DIGITS 4
#define FIELD3_DIGITS 4

/* Matrix (digit field) width: DIGITS*ADV_W
 * FIELD_W = 4*16 = 64px, matches label text width */
#define FIELD1_W (FIELD1_DIGITS * LED_DIGIT_W)
#define FIELD2_W (FIELD2_DIGITS * LED_DIGIT_W)
#define FIELD3_W (FIELD3_DIGITS * LED_DIGIT_W)

/* Label width: same as matrix width for left-aligned layout */
#define LABEL1_W FIELD1_W
#define LABEL2_W FIELD2_W
#define LABEL3_W FIELD3_W

#define BOTTOM5_DIGITS 5
#define BOTTOM5_W (BOTTOM5_DIGITS * LED_DIGIT_W + (BOTTOM5_DIGITS - 1) * LED_FIELD_GAP)

#define LED_BG_COLOR  ((lv_color_t)LV_COLOR_MAKE(0x08, 0x30, 0x39))
#define LED_FG_COLOR  ((lv_color_t)LV_COLOR_MAKE(0xff, 0xff, 0xff))

static int sim_steps = 8542;
static int sim_temp = 59;
static int sim_temp_hi = 63;
static int sim_humidity = 27;
static int sim_recovery = 5;
static int sim_last_hr = 80;
static int sim_week_min = 0;
static int sim_stress = 45;
static int sim_bodybatt = 68;
static int sim_battery = 85;

static int sunrise_hour = 1, sunrise_min = 18;
static int sunset_hour = 3, sunset_min = 13;

static void sim_update_data(void)
{
    sim_steps += (rand() % 5) + 1;
    sim_recovery = 4 + (rand() % 4);
    sim_last_hr = 75 + (rand() % 15);
    sim_stress = 30 + (rand() % 50);
    sim_bodybatt = 40 + (rand() % 50);
    sim_battery = 15 + (rand() % 85);
}

static lv_obj_t *root_page = NULL;
static lv_obj_t *dawn_label = NULL;
static lv_obj_t *dawn_time_label = NULL;
static lv_obj_t *moon_label = NULL;
static lv_obj_t *dusk_label = NULL;
static lv_obj_t *dusk_time_label = NULL;
static lv_obj_t *temp_label = NULL;
static lv_obj_t *weather_label = NULL;
static lv_obj_t *clock_bg = NULL;
static lv_obj_t *clock_col_h1 = NULL;
static lv_obj_t *clock_col_h2 = NULL;
static lv_obj_t *clock_col_colon = NULL;
static lv_obj_t *clock_col_m1 = NULL;
static lv_obj_t *clock_col_m2 = NULL;
static lv_obj_t *clock_lbl_digit_h1 = NULL;
static lv_obj_t *clock_lbl_digit_h2 = NULL;
static lv_obj_t *clock_lbl_digit_colon = NULL;
static lv_obj_t *clock_lbl_digit_m1 = NULL;
static lv_obj_t *clock_lbl_digit_m2 = NULL;
static lv_obj_t *clock_lbl_grid_h1 = NULL;
static lv_obj_t *clock_lbl_grid_h2 = NULL;
static lv_obj_t *clock_lbl_grid_colon = NULL;
static lv_obj_t *clock_lbl_grid_m1 = NULL;
static lv_obj_t *clock_lbl_grid_m2 = NULL;
static lv_obj_t *date_label = NULL;
static lv_obj_t *seconds_label = NULL;

static lv_obj_t *field1_label = NULL;
static lv_obj_t *field2_label = NULL;
static lv_obj_t *field3_label = NULL;

/* Per-character label arrays: each character position has its own bg+val label pair
 * This allows per-character text_color control:
 *   - Empty slots: bg='#' in DARK GREEN (template matrix), val=' ' (transparent)
 *   - Digit slots: bg='#' in WHITE (for segment color), val=digit in DARK GREEN (covers non-segments)
 *   - Dot slots: bg='.' in WHITE, val=' ' (lets white dot show through)
 */
static lv_obj_t *field1_bg_labels[FIELD1_DIGITS];
static lv_obj_t *field1_val_labels[FIELD1_DIGITS];
static lv_obj_t *field2_bg_labels[FIELD2_DIGITS];
static lv_obj_t *field2_val_labels[FIELD2_DIGITS];
static lv_obj_t *field3_bg_labels[FIELD3_DIGITS];
static lv_obj_t *field3_val_labels[FIELD3_DIGITS];

static lv_obj_t *bottom5_bg_labels[BOTTOM5_DIGITS];
static lv_obj_t *bottom5_val_labels[BOTTOM5_DIGITS];

static lv_obj_t *battery_container = NULL;
static lv_obj_t *battery_fill = NULL;
static lv_obj_t *battery_label = NULL;        /* 百分比标签（电池内部） */
static lv_obj_t *battery_percent_label = NULL; /* 百分比标签（电池外部） */
static int battery_display_mode = 0;           /* 0=不显示, 1=内部显示, 2=外部显示 */
static int moon_display_mode = 1;              /* 0=显示文字, 1=显示图片（默认图片） */

static lv_obj_t *stress_bar = NULL;
static lv_obj_t *bodybatt_bar = NULL;

static lv_timer_t *time_timer = NULL;
static lv_timer_t *sensor_timer = NULL;

/* Each clock column uses a 3-layer rendering stack (bottom to top):
 *
 *   Layer 0 (col bg):  solid clock_on (yellow)  →  segment base color
 *   Layer 1 (fg label): digit/':' in clock_off (dark green)  →  covers non-segment areas
 *   Layer 2 (grid label): '#' in black  →  grid lines & dots over everything
 *
 * Font glyph layout (lv_font_segments80, 42×80, 4bpp):
 *   - Opaque pixels (alpha 15) = drawn with text color
 *   - Transparent pixels (alpha 0) = shows layer below
 *   - '#' glyph: opaque = grid/dot pattern; transparent = gaps between grid
 *   - digit/':' glyphs: opaque = digit segments; transparent = non-segment areas
 *
 * Final visual result:
 *   Inside digit segments  → yellow base + black grid/dots (yellow shows through grid gaps)
 *   Outside digit segments → dark green + black grid/dots (dark green shows through grid gaps)
 *   Grid lines & dots everywhere → black (topmost layer) */

/* Column layout: 5 equal columns of 42px with 2px gaps
 * total = 5*42 + 4*2 = 218px, centered in 220px clock width → x offset 1 */
#define COL_W 42
#define COL_GAP 2
#define COL_TOTAL (5 * COL_W + 4 * COL_GAP)

static lv_obj_t *clock_col_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t w,
                                   const theme_colors_t *colors, const char *grid_char,
                                   lv_obj_t **digit_out, lv_obj_t **grid_out)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_pos(col, x, 0);
    lv_obj_set_size(col, w, CLOCK_H);
    lv_obj_set_style_bg_color(col, colors->clock_on, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(col, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(col, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(col, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(col, 0, LV_PART_MAIN);

    /* Layer 1 (digit label): digit/colon in clock_off — its opaque pixels
     * (non-segment areas) cover the yellow background with dark green;
     * transparent pixels (segment areas) let the yellow show through. */
    lv_obj_t *digit = lv_label_create(col);
    lv_obj_set_style_text_font(digit, &lv_font_segments80, LV_PART_MAIN);
    lv_obj_set_style_text_color(digit, colors->clock_off, LV_PART_MAIN);
    lv_obj_set_style_text_align(digit, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(digit, 0, 0);
    lv_obj_set_size(digit, w, CLOCK_H);
    lv_label_set_long_mode(digit, LV_LABEL_LONG_CLIP);

    /* Layer 2 (grid label): '#' in black — its opaque pixels draw the
     * grid lines & dots over everything; transparent pixels show the
     * layer below (yellow for segments, dark green for non-segments). */
    lv_obj_t *grid = lv_label_create(col);
    lv_obj_set_style_text_font(grid, &lv_font_segments80, LV_PART_MAIN);
    lv_obj_set_style_text_color(grid, (lv_color_t)LV_COLOR_MAKE(0x00, 0x00, 0x00), LV_PART_MAIN);
    lv_obj_set_style_text_align(grid, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(grid, 0, 0);
    lv_obj_set_size(grid, w, CLOCK_H);
    lv_label_set_long_mode(grid, LV_LABEL_LONG_CLIP);
    lv_label_set_text(grid, grid_char);

    *digit_out = digit;
    *grid_out = grid;
    return col;
}

static void led_char_create(lv_obj_t *parent, int x, int y, int w, int h,
                             lv_obj_t **bg_out, lv_obj_t **val_out)
{
    *bg_out = NULL;
    *val_out = NULL;

    /* Background label: shows '#' (all 35 dots) in DARK GREEN */
    lv_obj_t *bg = lv_label_create(parent);
    if (!bg) {
        return;
    }
    lv_obj_set_style_text_font(bg, FONT_LED, LV_PART_MAIN);
    lv_obj_set_style_text_color(bg, LED_BG_COLOR, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bg, (lv_color_t)LV_COLOR_MAKE(0x00, 0x00, 0x00), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_align(bg, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(bg, x, y);
    lv_obj_set_size(bg, w, h);
    lv_obj_set_style_opa(bg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(bg, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(bg, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_width(bg, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN);
    lv_label_set_long_mode(bg, LV_LABEL_LONG_CLIP);
    lv_label_set_text(bg, "#");

    /* Value label: shows the digit/decimal in WHITE, overlaid on top */
    lv_obj_t *val = lv_label_create(parent);
    if (!val) {
        *bg_out = bg;
        return;
    }
    lv_obj_set_style_text_font(val, FONT_LED, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, LED_FG_COLOR, LV_PART_MAIN);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(val, x, y);
    lv_obj_set_size(val, w, h);
    lv_obj_set_style_opa(val, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(val, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(val, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(val, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_width(val, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(val, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(val, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_label_set_long_mode(val, LV_LABEL_LONG_CLIP);

    *bg_out = bg;
    *val_out = val;
}

static void led_field_create(lv_obj_t *parent, int x, int y, int w, int h, int digits,
                              lv_obj_t **bg_labels, lv_obj_t **val_labels)
{
    int char_w = (w - (digits - 1) * LED_FIELD_GAP) / digits;
    for (int i = 0; i < digits; i++) {
        int cx = x + i * (char_w + LED_FIELD_GAP);
        led_char_create(parent, cx, y, char_w, h, &bg_labels[i], &val_labels[i]);
    }
}

static void led_field_set_value(lv_obj_t **bg_labels, lv_obj_t **val_labels,
                                 int digits, float value, int decimals)
{
    char src_buf[8];
    int total_chars;

    if (!bg_labels || !val_labels) return;

    if (decimals == 1) {
        snprintf(src_buf, sizeof(src_buf), "%.1f", (double)value);
        total_chars = strlen(src_buf);
    } else {
        snprintf(src_buf, sizeof(src_buf), "%d", (int)value);
        total_chars = strlen(src_buf);
    }

    for (int i = 0; i < digits; i++) {
        int src_idx = i - (digits - total_chars);
        if (src_idx < 0 || src_idx >= total_chars) {
            /* Empty position: bg shows dark green #, val shows nothing */
            if (bg_labels[i]) {
                lv_label_set_text(bg_labels[i], "#");
                lv_obj_set_style_text_color(bg_labels[i], LED_BG_COLOR, LV_PART_MAIN);
            }
            if (val_labels[i]) {
                lv_label_set_text(val_labels[i], " ");
                lv_obj_set_style_text_color(val_labels[i], LED_FG_COLOR, LV_PART_MAIN);
            }
        } else if (src_buf[src_idx] == '.') {
            /* Decimal point: bg shows dark green #, val shows white '.'
             * ('.' glyph is normal polarity: opaque=dot, so val white shows white dot) */
            if (bg_labels[i]) {
                lv_label_set_text(bg_labels[i], "#");
                lv_obj_set_style_text_color(bg_labels[i], LED_BG_COLOR, LV_PART_MAIN);
            }
            if (val_labels[i]) {
                lv_label_set_text(val_labels[i], ".");
                lv_obj_set_style_text_color(val_labels[i], LED_FG_COLOR, LV_PART_MAIN);
            }
        } else {
            /* Digit: LED font digits are INVERTED (opaque=non-segment, transparent=segment)
             *   bg # in WHITE → 35 white blocks (base layer)
             *   val digit in DARK GREEN → non-segments (opaque) covered dark green,
             *                              segments (transparent) show white from bg
             *   Result: segments=white, non-segments=dark green
             * NOTE: must pass a single NUL-terminated char, NOT &src_buf[src_idx],
             * otherwise lv_label_set_text reads the whole remaining string and center-
             * alignment + LV_LABEL_LONG_CLIP shows the MIDDLE char (e.g. "8698"→'6'/'9'),
             * causing every field to display the wrong digits. */
            char one_char[2] = { src_buf[src_idx], '\0' };
            if (bg_labels[i]) {
                lv_label_set_text(bg_labels[i], "#");
                lv_obj_set_style_text_color(bg_labels[i], LED_FG_COLOR, LV_PART_MAIN);
            }
            if (val_labels[i]) {
                lv_label_set_text(val_labels[i], one_char);
                lv_obj_set_style_text_color(val_labels[i], LED_BG_COLOR, LV_PART_MAIN);
            }
        }
    }
}

static const char *get_moon_string(int phase)
{
    /* 8 阶段映射到 4 个文字标签（每 2 个阶段归一组） */
    static const locale_str_id_t moon_ids[] = {
        LOCALE_STR_MOON_NEW,       /* phase 0: 新月 */
        LOCALE_STR_MOON_NEW,       /* phase 1: 蛾眉月 */
        LOCALE_STR_MOON_FIRST_Q,   /* phase 2: 上弦月 */
        LOCALE_STR_MOON_FIRST_Q,   /* phase 3: 盈凸月 */
        LOCALE_STR_MOON_FULL,      /* phase 4: 满月 */
        LOCALE_STR_MOON_FULL,      /* phase 5: 亏凸月 */
        LOCALE_STR_MOON_THIRD_Q,  /* phase 6: 下弦月 */
        LOCALE_STR_MOON_THIRD_Q,  /* phase 7: 残月 */
    };
    if (phase >= 0 && phase < 8) {
        return locale_get_string(moon_ids[phase]);
    }
    return "";
}

static int get_moon_phase(int year, int month, int day)
{
    int r = year % 100;
    r %= 19;
    if (r > 9) r -= 19;
    r = ((r * 11) % 30) + month + day;
    if (month < 3) r += 2;
    r -= (year < 2000) ? 4 : 8;
    r = r % 30;
    if (r < 0) r += 30;
    return (r * 8) / 30;
}

static void time_update_cb(lv_timer_t *timer)
{
    watchface_update_time();
    watchface_update_date();
    ARG_UNUSED(timer);
}

static void sensor_update_cb(lv_timer_t *timer)
{
    sim_update_data();
    watchface_update_sensors();
    ARG_UNUSED(timer);
}

void watchface_start(void)
{
    theme_init(THEME_YELLOW);
    const theme_colors_t *colors = theme_get_colors();
    printk("watchface_start: theme initialized\n");

    lv_obj_set_style_bg_color(lv_scr_act(), colors->bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(lv_scr_act(), 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(lv_scr_act(), 0, LV_PART_MAIN);
    printk("watchface_start: screen bg set\n");

    root_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(root_page, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(root_page, colors->bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_page, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(root_page, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root_page, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(root_page, 0, LV_PART_MAIN);
    lv_obj_center(root_page);
    printk("watchface_start: root page created\n");

    /* Top: DAWN label + time | moon | DUSK label + time
     * Row 1 (y=8):  DAWN:[moon]:DUSK   （标签紧贴moon）
     * Row 2 (y=19): 01:18 [NEW] 03:13  （时间紧贴moon） */
    dawn_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(dawn_label, FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(dawn_label, (lv_color_t)LV_COLOR_MAKE(0x52, 0xaa, 0xac), LV_PART_MAIN);
    lv_obj_set_style_text_align(dawn_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dawn_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(dawn_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(dawn_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dawn_label, 0, LV_PART_MAIN);
    lv_obj_set_pos(dawn_label, 64, 8);
    lv_obj_set_width(dawn_label, 40);

    dawn_time_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(dawn_time_label, FONT_TIME_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(dawn_time_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_align(dawn_time_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dawn_time_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(dawn_time_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(dawn_time_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dawn_time_label, 0, LV_PART_MAIN);
    lv_obj_set_pos(dawn_time_label, 64, 16);
    lv_obj_set_width(dawn_time_label, 40);

    moon_label = lv_label_create(root_page);
    /* 默认图片模式：20×20 月相图。参考 Segment34.CN：
     *   dc.setColor(themeColors[moon], COLOR_TRANSPARENT) → 前景=moon色，背景=透明
     * 月相图片字形中亮像素(月亮)用 text_color 绘制，暗像素透明透出表盘深色背景。 */
    lv_obj_set_style_text_font(moon_label, FONT_MOON_IMAGE, LV_PART_MAIN);
    lv_obj_set_style_text_color(moon_label, colors->moon, LV_PART_MAIN);
    lv_obj_set_style_text_align(moon_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(moon_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(moon_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(moon_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(moon_label, 0, LV_PART_MAIN);
    lv_obj_set_pos(moon_label, CENTER_X - 10, 7);
    lv_obj_set_width(moon_label, 20);
    lv_label_set_long_mode(moon_label, LV_LABEL_LONG_CLIP);   /* 强制单行不换行 */

    dusk_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(dusk_label, FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(dusk_label, (lv_color_t)LV_COLOR_MAKE(0x52, 0xaa, 0xac), LV_PART_MAIN);
    lv_obj_set_style_text_align(dusk_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dusk_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(dusk_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(dusk_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dusk_label, 0, LV_PART_MAIN);
    lv_obj_set_pos(dusk_label, 136, 8);
    lv_obj_set_width(dusk_label, 40);

    dusk_time_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(dusk_time_label, FONT_TIME_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(dusk_time_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_align(dusk_time_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dusk_time_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(dusk_time_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(dusk_time_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dusk_time_label, 0, LV_PART_MAIN);
    lv_obj_set_pos(dusk_time_label, 136, 16);
    lv_obj_set_width(dusk_time_label, 40);

    /* Weather: temp line + description line
     * Row 3 (y=40):  59F, ↑4, 27%
     * Row 4 (y=58):  PARTLY CLOUDY  */
    temp_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(temp_label, FONT_MED, LV_PART_MAIN);
    lv_obj_set_style_text_color(temp_label, colors->text, LV_PART_MAIN);
    lv_obj_set_style_text_align(temp_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(temp_label, SCREEN_W);
    lv_obj_set_pos(temp_label, 0, 30);

    weather_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(weather_label, FONT_MED, LV_PART_MAIN);
    lv_obj_set_style_text_color(weather_label, colors->weather, LV_PART_MAIN);
    lv_obj_set_style_text_align(weather_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(weather_label, SCREEN_W);
    lv_obj_set_pos(weather_label, 0, 45);

    /* Large clock — 5 equal columns, 3-layer rendering per column:
     *   Layer 0 (col bg):  solid clock_on (yellow)  →  segment base
     *   Layer 1 (digit label): digit/':' in clock_off (dark green)  →  non-segment fill
     *   Layer 2 (grid label): '#' in black  →  grid/dots over everything
     * The font (lv_font_segments80) includes '#' as full-column grid/dot
     * pattern, digits 0-9, and ':' — all 42×80 with cut-out segments. */
    clock_bg = lv_obj_create(root_page);
    lv_obj_set_pos(clock_bg, CLOCK_X, CLOCK_Y);
    lv_obj_set_size(clock_bg, CLOCK_W, CLOCK_H);
    lv_obj_set_style_bg_opa(clock_bg, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(clock_bg, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(clock_bg, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(clock_bg, 0, LV_PART_MAIN);

    /* 5 equal columns: 42px wide, 2px gap, total = 218px, centered in 220px */
    lv_coord_t col_x0 = (CLOCK_W - COL_TOTAL) / 2;
    clock_col_h1 = clock_col_create(clock_bg, col_x0 + 0 * (COL_W + COL_GAP), COL_W,
                     colors, "#", &clock_lbl_digit_h1, &clock_lbl_grid_h1);
    clock_col_h2 = clock_col_create(clock_bg, col_x0 + 1 * (COL_W + COL_GAP), COL_W,
                     colors, "#", &clock_lbl_digit_h2, &clock_lbl_grid_h2);
    clock_col_colon = clock_col_create(clock_bg, col_x0 + 2 * (COL_W + COL_GAP), COL_W,
                     colors, "#", &clock_lbl_digit_colon, &clock_lbl_grid_colon);
    clock_col_m1 = clock_col_create(clock_bg, col_x0 + 3 * (COL_W + COL_GAP), COL_W,
                     colors, "#", &clock_lbl_digit_m1, &clock_lbl_grid_m1);
    clock_col_m2 = clock_col_create(clock_bg, col_x0 + 4 * (COL_W + COL_GAP), COL_W,
                     colors, "#", &clock_lbl_digit_m2, &clock_lbl_grid_m2);

    /* Initial digit text */
    lv_label_set_text(clock_lbl_digit_h1, "0");
    lv_label_set_text(clock_lbl_digit_h2, "0");
    lv_label_set_text(clock_lbl_digit_colon, ":");
    lv_label_set_text(clock_lbl_digit_m1, "0");
    lv_label_set_text(clock_lbl_digit_m2, "0");
    printk("watchface_start: clock created\n");

    /* Left stress indicator — height proportional to stress level (0-100) */
    stress_bar = lv_obj_create(root_page);
    int stress_h = (CLOCK_H * sim_stress) / 100;
    if (stress_h < 2) stress_h = 2;
    lv_obj_set_size(stress_bar, 3, stress_h);
    lv_obj_set_pos(stress_bar, CLOCK_X - 5, CLOCK_Y + CLOCK_H - stress_h);
    lv_obj_set_style_bg_color(stress_bar, colors->stress, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(stress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(stress_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(stress_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(stress_bar, 0, LV_PART_MAIN);
    printk("watchface_start: stress bar created\n");

    /* Right body battery indicator — height proportional to body battery (0-100) */
    bodybatt_bar = lv_obj_create(root_page);
    int bodybatt_h = (CLOCK_H * sim_bodybatt) / 100;
    if (bodybatt_h < 2) bodybatt_h = 2;
    lv_obj_set_size(bodybatt_bar, 3, bodybatt_h);
    lv_obj_set_pos(bodybatt_bar, CLOCK_X + CLOCK_W + 2, CLOCK_Y + CLOCK_H - bodybatt_h);
    lv_obj_set_style_bg_color(bodybatt_bar, colors->bodybatt, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bodybatt_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bodybatt_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bodybatt_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bodybatt_bar, 0, LV_PART_MAIN);
    printk("watchface_start: body batt bar created\n");

    /* Date line + seconds — full width matching clock
     * Row 6: MON, 5 MAY 2025         32
     * 日期行也使用实体线字体 FONT_DATA = montserrat_12 */
    date_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(date_label, FONT_DATA, LV_PART_MAIN);
    lv_obj_set_style_text_color(date_label, colors->text, LV_PART_MAIN);
    lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(date_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(date_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(date_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(date_label, 0, LV_PART_MAIN);
    lv_obj_set_pos(date_label, CLOCK_X, CLOCK_Y + CLOCK_H + 4);
    lv_obj_set_width(date_label, CLOCK_W);

    seconds_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(seconds_label, FONT_DATA, LV_PART_MAIN);
    lv_obj_set_style_text_color(seconds_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_align(seconds_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(seconds_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(seconds_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(seconds_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(seconds_label, 0, LV_PART_MAIN);
    lv_obj_set_pos(seconds_label, CLOCK_X + CLOCK_W - 30, CLOCK_Y + CLOCK_H + 4);
    lv_obj_set_width(seconds_label, 30);

    /* Three data fields: label on top (solid font), LED dot-matrix value below
     * Row 7: RECOVERY HRS:   LAST HR:   WEEK ACT MIN:   （标签 montserrat_8）
     * Row 8:     5.0           80           0             （数值 LED 点阵风格） */
    int field_top = CLOCK_Y + CLOCK_H + 20;
    int field_h = LED_DIGIT_H;
    int label_h = 10;
    int field_gap = 10;  /* 字段间间距：10像素 */
    int total_field_w = 3 * LABEL1_W + 2 * field_gap;
    int field_start_x = CENTER_X - total_field_w / 2;

    int field1_x = field_start_x;
    int field2_x = field_start_x + LABEL1_W + field_gap;
    int field3_x = field_start_x + 2 * (LABEL1_W + field_gap);

    /* ========= Field 1: RECOVERY HRS ========= */
    field1_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(field1_label, FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(field1_label, (lv_color_t)LV_COLOR_MAKE(0x52, 0xaa, 0xac), LV_PART_MAIN);
    lv_obj_set_style_text_align(field1_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(field1_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(field1_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(field1_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(field1_label, 0, LV_PART_MAIN);
    lv_label_set_long_mode(field1_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(field1_label, field1_x, field_top);
    lv_obj_set_width(field1_label, LABEL1_W);

    led_field_create(root_page, field1_x, field_top + label_h + 3, FIELD1_W, field_h, FIELD1_DIGITS, field1_bg_labels, field1_val_labels);

    /* ========= Field 2: LAST HR ========= */
    field2_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(field2_label, FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(field2_label, (lv_color_t)LV_COLOR_MAKE(0x52, 0xaa, 0xac), LV_PART_MAIN);
    lv_obj_set_style_text_align(field2_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(field2_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(field2_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(field2_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(field2_label, 0, LV_PART_MAIN);
    lv_label_set_long_mode(field2_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(field2_label, field2_x, field_top);
    lv_obj_set_width(field2_label, LABEL2_W);

    led_field_create(root_page, field2_x, field_top + label_h + 3, FIELD2_W, field_h, FIELD2_DIGITS, field2_bg_labels, field2_val_labels);

    /* ========= Field 3: WEEK ACT MIN ========= */
    field3_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(field3_label, FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(field3_label, (lv_color_t)LV_COLOR_MAKE(0x52, 0xaa, 0xac), LV_PART_MAIN);
    lv_obj_set_style_text_align(field3_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(field3_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(field3_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(field3_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(field3_label, 0, LV_PART_MAIN);
    lv_label_set_long_mode(field3_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(field3_label, field3_x, field_top);
    lv_obj_set_width(field3_label, LABEL3_W);

    led_field_create(root_page, field3_x, field_top + label_h + 3, FIELD3_W, field_h, FIELD3_DIGITS, field3_bg_labels, field3_val_labels);
    printk("watchface_start: fields created\n");

    /* Bottom: icon + 5-digit steps (LED font) + icon
     * Row 9:  ♥   0 8 5 7 3   🔥   */
    int bottom_y = field_top + label_h + 3 + field_h + 5;
    printk("watchface_start: creating heart icon\n");
    icon_draw(root_page, ICON_HEART, 30, bottom_y + 2, colors->heart_rate);
    printk("watchface_start: heart icon created\n");

    int bottom5_x = CENTER_X - BOTTOM5_W / 2;
    printk("watchface_start: creating bottom5 field\n");
    led_field_create(root_page, bottom5_x, bottom_y, BOTTOM5_W, LED_DIGIT_H, BOTTOM5_DIGITS, bottom5_bg_labels, bottom5_val_labels);
    printk("watchface_start: bottom5 field created\n");

    icon_draw(root_page, ICON_CALORIES, 200, bottom_y + 2, colors->accent);
    printk("watchface_start: bottom icons created\n");

    /* Battery icon — dynamic with fill based on battery level
     * Row 10: centered at bottom, showing battery level 0-100% */
    int battery_y = SCREEN_H - 17;
    int battery_w = 24;
    int battery_h = 12;
    
    battery_container = lv_obj_create(root_page);
    lv_obj_set_size(battery_container, battery_w, battery_h);
    lv_obj_set_pos(battery_container, CENTER_X - battery_w / 2, battery_y);
    lv_obj_set_style_bg_opa(battery_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(battery_container, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(battery_container, (lv_color_t)LV_COLOR_MAKE(0xa0, 0xa0, 0xa0), LV_PART_MAIN);
    lv_obj_set_style_radius(battery_container, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(battery_container, 0, LV_PART_MAIN);
    
    battery_fill = lv_obj_create(battery_container);
    /* Fill height = content_h - 2*gap = (battery_h - 2*border) - 2*gap
     * = (12 - 2) - 4 = 6, so 2px gaps on top AND bottom (vertically centered) */
    lv_obj_set_height(battery_fill, battery_h - 6);
    lv_obj_set_width(battery_fill, 0);
    lv_obj_set_pos(battery_fill, 2, 2);
    lv_obj_set_style_bg_color(battery_fill, colors->battery, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(battery_fill, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(battery_fill, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(battery_fill, 0, LV_PART_MAIN);
    
    battery_label = lv_label_create(battery_container);
    lv_label_set_text(battery_label, "");
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_8, LV_PART_MAIN);
    lv_obj_set_style_text_color(battery_label, (lv_color_t)LV_COLOR_MAKE(0x00, 0x00, 0x00), LV_PART_MAIN);
    lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(battery_label, 0, LV_PART_MAIN);
    lv_obj_set_size(battery_label, battery_w, battery_h);
    lv_obj_set_style_bg_opa(battery_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);

    /* Battery percentage label (outside, to the right of battery cap) */
    battery_percent_label = lv_label_create(root_page);
    lv_label_set_text(battery_percent_label, "");
    lv_obj_set_style_text_font(battery_percent_label, &lv_font_montserrat_8, LV_PART_MAIN);
    lv_obj_set_style_text_color(battery_percent_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_align(battery_percent_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_pad_all(battery_percent_label, 0, LV_PART_MAIN);
    lv_obj_set_pos(battery_percent_label, CENTER_X + battery_w / 2 + 5, battery_y + 2);
    lv_obj_add_flag(battery_percent_label, LV_OBJ_FLAG_HIDDEN);

    /* Battery cap */
    lv_obj_t *battery_cap = lv_obj_create(root_page);
    lv_obj_set_size(battery_cap, 3, 6);
    lv_obj_set_pos(battery_cap, CENTER_X + battery_w / 2, battery_y + 3);
    lv_obj_set_style_bg_color(battery_cap, (lv_color_t)LV_COLOR_MAKE(0xa0, 0xa0, 0xa0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(battery_cap, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(battery_cap, 1, LV_PART_MAIN);
    lv_obj_set_style_border_width(battery_cap, 0, LV_PART_MAIN);

    watchface_update_time();
    printk("watchface_start: time updated\n");
    watchface_update_date();
    printk("watchface_start: date updated\n");
    watchface_update_weather();
    printk("watchface_start: weather updated\n");
    watchface_update_sensors();
    printk("watchface_start: sensors updated\n");

    time_timer = lv_timer_create(time_update_cb, 1000, NULL);
    sensor_timer = lv_timer_create(sensor_update_cb, 10000, NULL);
    printk("watchface_start: timers created, done\n");
}

void watchface_stop(void)
{
    if (time_timer) { lv_timer_del(time_timer); time_timer = NULL; }
    if (sensor_timer) { lv_timer_del(sensor_timer); sensor_timer = NULL; }
    if (root_page) { lv_obj_del(root_page); root_page = NULL; }
}

void watchface_update_time(void)
{
    if (!clock_lbl_digit_h1 || !clock_lbl_digit_h2 || !clock_lbl_digit_m1 || 
        !clock_lbl_digit_m2 || !clock_lbl_digit_colon) return;

    struct tm timeinfo;
    time_t now = time(NULL);
    localtime_r(&now, &timeinfo);

    char dig[2];
    dig[0] = '0' + (timeinfo.tm_hour / 10);
    dig[1] = '\0';
    lv_label_set_text(clock_lbl_digit_h1, dig);
    dig[0] = '0' + (timeinfo.tm_hour % 10);
    lv_label_set_text(clock_lbl_digit_h2, dig);
    dig[0] = '0' + (timeinfo.tm_min / 10);
    lv_label_set_text(clock_lbl_digit_m1, dig);
    dig[0] = '0' + (timeinfo.tm_min % 10);
    lv_label_set_text(clock_lbl_digit_m2, dig);

    /* Colon always visible — dot areas transparent showing yellow,
     * non-dot areas dark green. The black grid layer stays always visible. */
    lv_label_set_text(clock_lbl_digit_colon, ":");
}

void watchface_update_date(void)
{
    if (!date_label || !seconds_label || !dawn_label || !dusk_label || 
        !dawn_time_label || !dusk_time_label || !moon_label) return;

    struct tm timeinfo;
    time_t now = time(NULL);
    localtime_r(&now, &timeinfo);

    /* 日期行：ZH 用 "周X yyyy-MM-dd"，EN 用 "MON, 5 MAY 2025" */
    char date_str[48];
    if (current_lang == LANG_ZH) {
        static const char *zhou[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
        snprintf(date_str, sizeof(date_str), "%s %04d-%02d-%02d",
                 zhou[timeinfo.tm_wday],
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
        lv_obj_set_style_text_font(date_label, FONT_CJK, LV_PART_MAIN);
    } else {
        const char *weekday_str;
        if (timeinfo.tm_wday == 0) {
            weekday_str = locale_get_string(LOCALE_STR_SUNDAY);
        } else {
            weekday_str = locale_get_string(LOCALE_STR_MONDAY + timeinfo.tm_wday - 1);
        }
        snprintf(date_str, sizeof(date_str), "%s, %d %s %d",
                 weekday_str, timeinfo.tm_mday,
                 locale_get_string(LOCALE_STR_JANUARY + timeinfo.tm_mon),
                 timeinfo.tm_year + 1900);
        lv_obj_set_style_text_font(date_label, FONT_DATA, LV_PART_MAIN);
    }
    lv_label_set_text(date_label, date_str);
    LOG_INF("[DATE] %s (wday=%d)", date_str, timeinfo.tm_wday);

    /* seconds_label 显示当前时间的秒（0-59），每秒更新 */
    char sec_str[8];
    snprintf(sec_str, sizeof(sec_str), "%02d", timeinfo.tm_sec);
    lv_label_set_text(seconds_label, sec_str);

    int moon_phase = get_moon_phase(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);

    /* DAWN/DUSK 始终英文 */
    lv_label_set_text(dawn_label, "DAWN:");
    lv_label_set_text(dusk_label, "DUSK:");

    char dawn_time_str[16];
    char dusk_time_str[16];
    snprintf(dawn_time_str, sizeof(dawn_time_str), "%02d:%02d", sunrise_hour, sunrise_min);
    snprintf(dusk_time_str, sizeof(dusk_time_str), "%02d:%02d", sunset_hour, sunset_min);
    lv_label_set_text(dawn_time_label, dawn_time_str);
    lv_label_set_text(dusk_time_label, dusk_time_str);

    /* 月相显示：0=文字，1=图片（默认）。参考 Segment34.CN moonPhase() 返回 "0"-"7" */
    if (moon_display_mode == 0) {
        lv_obj_set_style_text_font(moon_label, FONT_MOON, LV_PART_MAIN);
        lv_obj_set_width(moon_label, 32);
        lv_obj_set_pos(moon_label, CENTER_X - 16, 13);
        lv_label_set_text(moon_label, get_moon_string(moon_phase));
    } else {
        lv_obj_set_style_text_font(moon_label, FONT_MOON_IMAGE, LV_PART_MAIN);
        lv_obj_set_width(moon_label, 20);
        lv_obj_set_pos(moon_label, CENTER_X - 10, 7);
        char moon_char[2] = { '0' + moon_phase, '\0' };
        lv_label_set_text(moon_label, moon_char);
    }
}

void watchface_update_weather(void)
{
    if (!temp_label || !weather_label) return;

    if (current_lang == LANG_ZH) {
        /* 第三行：天气描述 + 温度 */
        char temp_str[32];
        snprintf(temp_str, sizeof(temp_str), "%s %dF",
                 locale_get_string(LOCALE_STR_PARTLY_CLOUDY), sim_temp);
        lv_obj_set_style_text_font(temp_label, FONT_CJK, LV_PART_MAIN);
        lv_label_set_text(temp_label, temp_str);
        LOG_INF("[WEATHER] %s (temp=%dF)", temp_str, sim_temp);

        /* 第四行：农历日期 + 节气 */
        struct tm timeinfo;
        time_t now = time(NULL);
        localtime_r(&now, &timeinfo);
        lunar_date_t lunar;
        lunar_calendar_convert(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                               timeinfo.tm_mday, &lunar);
        char lunar_str[48];
        if (lunar.jieqi[0] != '\0') {
            snprintf(lunar_str, sizeof(lunar_str), "%s%s %s",
                     lunar.month_name, lunar.day_name, lunar.jieqi);
        } else {
            snprintf(lunar_str, sizeof(lunar_str), "%s%s",
                     lunar.month_name, lunar.day_name);
        }
        lv_obj_set_style_text_font(weather_label, FONT_CJK, LV_PART_MAIN);
        lv_label_set_text(weather_label, lunar_str);
        LOG_INF("[LUNAR] %s%s (jieqi='%s') year=%d month=%d day=%d leap=%d",
                lunar.month_name, lunar.day_name, lunar.jieqi,
                lunar.year, lunar.month, lunar.day, lunar.leap_month);
    } else {
        char temp_str[32];
        snprintf(temp_str, sizeof(temp_str), "%dF, +%d, %d%%",
                 sim_temp, sim_temp_hi - sim_temp, sim_humidity);
        lv_obj_set_style_text_font(temp_label, FONT_MED, LV_PART_MAIN);
        lv_label_set_text(temp_label, temp_str);

        lv_obj_set_style_text_font(weather_label, FONT_MED, LV_PART_MAIN);
        lv_label_set_text(weather_label, "PARTLY CLOUDY");
        LOG_INF("[WEATHER] %s | PARTLY CLOUDY", temp_str);
    }
}

void watchface_update_battery(void)
{
    if (!battery_fill) return;

    int battery_level = sim_battery;
    if (battery_level < 0) battery_level = 0;
    if (battery_level > 100) battery_level = 100;

    /* Fill width proportional to battery level (max 20px inside 24px container) */
    int fill_w = (battery_level * 20) / 100;
    if (fill_w < 1 && battery_level > 0) fill_w = 1;

    lv_obj_set_width(battery_fill, fill_w);

    /* Color: red for 1-10%, green for 90-100%, white otherwise */
    if (battery_level <= 10) {
        lv_obj_set_style_bg_color(battery_fill, (lv_color_t)LV_COLOR_MAKE(0xff, 0x00, 0x00), LV_PART_MAIN);
    } else if (battery_level >= 90) {
        lv_obj_set_style_bg_color(battery_fill, (lv_color_t)LV_COLOR_MAKE(0x00, 0xff, 0x00), LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(battery_fill, (lv_color_t)LV_COLOR_MAKE(0xff, 0xff, 0xff), LV_PART_MAIN);
    }

    /* Always show gray border (including 90-100%) */
    lv_obj_set_style_border_width(battery_container, 1, LV_PART_MAIN);

    /* Percentage display mode: 0=hidden, 1=inside battery, 2=outside battery */
    char percent_str[4];
    snprintf(percent_str, sizeof(percent_str), "%d", battery_level);

    if (battery_display_mode == 1) {
        /* Inside: show label inside battery container */
        lv_label_set_text(battery_label, percent_str);
        lv_obj_clear_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(battery_percent_label, LV_OBJ_FLAG_HIDDEN);
    } else if (battery_display_mode == 2) {
        /* Outside: show label to the right of battery */
        lv_label_set_text(battery_percent_label, percent_str);
        lv_obj_clear_flag(battery_percent_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        /* Hidden */
        lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(battery_percent_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void watchface_update_sensors(void)
{
    if (!field1_label || !field2_label || !field3_label) return;
    if (!stress_bar || !bodybatt_bar) return;

    lv_label_set_text(field1_label, "RECOVERY HRS:");
    led_field_set_value(field1_bg_labels, field1_val_labels, FIELD1_DIGITS, (float)sim_recovery + 0.0f, 1);

    lv_label_set_text(field2_label, "LAST HR:");
    led_field_set_value(field2_bg_labels, field2_val_labels, FIELD2_DIGITS, (float)sim_last_hr, 0);

    lv_label_set_text(field3_label, "WEEK ACT MIN:");
    led_field_set_value(field3_bg_labels, field3_val_labels, FIELD3_DIGITS, (float)sim_week_min, 0);

    led_field_set_value(bottom5_bg_labels, bottom5_val_labels, BOTTOM5_DIGITS, (float)sim_steps, 0);

    int max_h = CLOCK_H;
    int stress_h = (max_h * sim_stress) / 100;
    int bodybatt_h = (max_h * sim_bodybatt) / 100;
    if (stress_h < 2) stress_h = 2;
    if (bodybatt_h < 2) bodybatt_h = 2;

    lv_obj_set_height(stress_bar, stress_h);
    lv_obj_set_y(stress_bar, CLOCK_Y + CLOCK_H - stress_h);

    lv_obj_set_height(bodybatt_bar, bodybatt_h);
    lv_obj_set_y(bodybatt_bar, CLOCK_Y + CLOCK_H - bodybatt_h);

    watchface_update_weather();
    watchface_update_battery();
}

void watchface_switch_battery_display(void)
{
    /* Cycle: 0=不显示 → 1=内部显示 → 2=外部显示 → 0 */
    battery_display_mode = (battery_display_mode + 1) % 3;
    watchface_update_battery();
}

void watchface_switch_moon_display(void)
{
    /* Cycle: 0=文字 → 1=图片 → 0 */
    moon_display_mode = (moon_display_mode + 1) % 2;
    watchface_update_date();
}

void watchface_switch_language(void)
{
    if (current_lang == LANG_ZH) {
        locale_set_current(LANG_EN);
    } else {
        locale_set_current(LANG_ZH);
    }
    watchface_update_date();
    watchface_update_sensors();
}

void watchface_switch_theme(void)
{
    theme_color_t current = theme_get_current();
    theme_color_t next = (current + 1) % THEME_COUNT;
    theme_set_current(next);

    const theme_colors_t *colors = theme_get_colors();

    lv_obj_set_style_bg_color(lv_scr_act(), colors->bg, LV_PART_MAIN);
    lv_obj_set_style_bg_color(root_page, colors->bg, LV_PART_MAIN);

    /* Update clock column backgrounds (Layer 0: solid clock_on — segment base) */
    lv_obj_set_style_bg_color(clock_col_h1, colors->clock_on, LV_PART_MAIN);
    lv_obj_set_style_bg_color(clock_col_h2, colors->clock_on, LV_PART_MAIN);
    lv_obj_set_style_bg_color(clock_col_colon, colors->clock_on, LV_PART_MAIN);
    lv_obj_set_style_bg_color(clock_col_m1, colors->clock_on, LV_PART_MAIN);
    lv_obj_set_style_bg_color(clock_col_m2, colors->clock_on, LV_PART_MAIN);

    /* Update clock digit labels (Layer 1: digit/':' in clock_off — non-segment fill) */
    lv_obj_set_style_text_color(clock_lbl_digit_h1, colors->clock_off, LV_PART_MAIN);
    lv_obj_set_style_text_color(clock_lbl_digit_h2, colors->clock_off, LV_PART_MAIN);
    lv_obj_set_style_text_color(clock_lbl_digit_colon, colors->clock_off, LV_PART_MAIN);
    lv_obj_set_style_text_color(clock_lbl_digit_m1, colors->clock_off, LV_PART_MAIN);
    lv_obj_set_style_text_color(clock_lbl_digit_m2, colors->clock_off, LV_PART_MAIN);

    /* Clock grid labels (Layer 2: '#' in black — grid/dots stay black;
     * no color update needed). */

    lv_obj_set_style_text_color(dawn_label, (lv_color_t)LV_COLOR_MAKE(0x52, 0xaa, 0xac), LV_PART_MAIN);
    lv_obj_set_style_text_color(dawn_time_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_color(dusk_label, (lv_color_t)LV_COLOR_MAKE(0x52, 0xaa, 0xac), LV_PART_MAIN);
    lv_obj_set_style_text_color(dusk_time_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_color(moon_label, colors->moon, LV_PART_MAIN);
    lv_obj_set_style_text_color(temp_label, colors->text, LV_PART_MAIN);
    lv_obj_set_style_text_color(weather_label, colors->weather, LV_PART_MAIN);
    lv_obj_set_style_text_color(date_label, colors->text, LV_PART_MAIN);
    lv_obj_set_style_text_color(seconds_label, colors->data_val, LV_PART_MAIN);

    lv_obj_set_style_text_color(field1_label, (lv_color_t)LV_COLOR_MAKE(0x52, 0xaa, 0xac), LV_PART_MAIN);
    lv_obj_set_style_text_color(field2_label, (lv_color_t)LV_COLOR_MAKE(0x52, 0xaa, 0xac), LV_PART_MAIN);
    lv_obj_set_style_text_color(field3_label, (lv_color_t)LV_COLOR_MAKE(0x52, 0xaa, 0xac), LV_PART_MAIN);

    lv_obj_set_style_bg_color(stress_bar, colors->stress, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bodybatt_bar, colors->bodybatt, LV_PART_MAIN);
}
