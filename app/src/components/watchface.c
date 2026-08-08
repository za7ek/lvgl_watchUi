#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watchface.h"
#include "lunar_calendar.h"
#include "locale.h"
#include "theme.h"
#include "lv_font_cjk.h"
#include "lv_font_segments80.h"
#include "lv_font_led.h"
#include "lv_font_xsmol.h"
#include "lv_font_moon.h"
#include "lv_font_icons.h"
#include "settings.h"
#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <time.h>

LOG_MODULE_REGISTER(watchface, LOG_LEVEL_INF);

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120

/* ---------------------------------------------------------------------------
 * 圆形可视区
 *
 * GC9A01 是 240×240 的圆屏：帧缓冲是方的，但只有以 (119.5, 119.5) 为心、
 * R=120 的圆内能看见，四角是物理上不存在的。表壳还会再压掉一两个像素，
 * 所以布局按 SAFE_R = 117（离屏边 3px）收：任何一个绘制像素到圆心的距离
 * 都不许超过它。判据是"元素四角"，不是"元素宽度"——同样宽的一行，越靠近
 * 上下边缘越容易被切。
 *
 * 校核方法见 docs/DEVELOPMENT.md：用 native_sim 抓帧，对每个非背景像素算
 * hypot(x-119.5, y-119.5) 取最大值。
 * ------------------------------------------------------------------------- */
#define SAFE_R 117

/* 时钟块：4 个 42px 数字列 + 1 个窄冒号列。冒号本来就不需要一个数字那么宽，
 * 收窄它把整块从 218px 压到 194px —— 上两角的所需半径从 123.7 降到 113.8，
 * 这是把时钟塞进圆里代价最小的一刀（不用改 42×80 的字体，也不用下移）。 */
#define COL_W    42
#define COL_GAP  2
#define COLON_W  18
#define COL_TOTAL (4 * COL_W + COLON_W + 4 * COL_GAP)   /* 194 */

#define CLOCK_W COL_TOTAL
#define CLOCK_H 80
#define CLOCK_X (CENTER_X - CLOCK_W / 2)   /* 23 */
#define CLOCK_Y 60

/* 列在时钟块内的 x 偏移 */
#define COL_X_H1    0
#define COL_X_H2    (COL_X_H1 + COL_W + COL_GAP)
#define COL_X_COLON (COL_X_H2 + COL_W + COL_GAP)
#define COL_X_M1    (COL_X_COLON + COLON_W + COL_GAP)
#define COL_X_M2    (COL_X_M1 + COL_W + COL_GAP)

/* 月相 20×20 图片的左上角 Y。文字模式另有一套坐标（见 watchface_update_date）。 */
#define MOON_Y 10

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
/* CJK 中文字体：13px / 4bpp，line_height 与 base_line 同 montserrat_12，
 * 因此中文行与英文行占用完全相同的垂直空间。用于日期行/天气行/农历行。 */
#define FONT_CJK        &lv_font_cjk
#define FONT_LED        &lv_font_led   /* 用于字段数值行 + 倒数第二行steps（LED点阵风格） */
#define FONT_ICONS      &lv_font_icons /* 状态图标：闹钟/勿扰/蓝牙/久坐提醒，21px */

#define LED_DIGIT_W 16    /* adv_w=16px (14px char + 2px gap) */
#define LED_DIGIT_H 20    /* 2x2 blocks, 1px gaps → 20px tall */
#define LED_FIELD_GAP 0   /* no extra gap; spacing is in adv_w */

/* 三个数值字段每个 3 格。原来是 4 格：3*4*16 = 192px 宽的一行落在 y=173..192，
 * 那里圆的可用宽度只有 183px（SAFE_R=117 时），左右两个字段各被切掉约 8px ——
 * 而且这行再怎么挪也躲不开，它已经在时钟和步数之间唯一的空档里。
 * 3 格够用：RECOVERY "5.0"、LAST HR 三位、WEEK ACT MIN 三位；超出量程时
 * led_field_set_value() 会先丢小数、再退化成全 9，不会显示成截断后的错数。 */
#define FIELD1_DIGITS 3
#define FIELD2_DIGITS 3
#define FIELD3_DIGITS 3

/* Matrix (digit field) width: DIGITS*ADV_W = 3*16 = 48px */
#define FIELD1_W (FIELD1_DIGITS * LED_DIGIT_W)
#define FIELD2_W (FIELD2_DIGITS * LED_DIGIT_W)
#define FIELD3_W (FIELD3_DIGITS * LED_DIGIT_W)

/* 字段间距。标签（"RECOVERY HRS:" 约 52px）比 48px 的点阵宽，所以标签宽度
 * 单独给到 = 点阵宽 + 间距，正好铺满一个字段的步进，不会被 CLIP 截掉。 */
#define FIELD_GAP 16
#define LABEL1_W (FIELD1_W + FIELD_GAP)
#define LABEL2_W (FIELD2_W + FIELD_GAP)
#define LABEL3_W (FIELD3_W + FIELD_GAP)

/* 日期行：右侧秒数标签占 SECONDS_W，左侧也预留同样宽度，日期在中间剩余区域
 * 内居中 —— 这样日期的中心与表盘中心重合。中英文使用同一套布局。 */
#define SECONDS_W 30
#define DATE_W    (CLOCK_W - 2 * SECONDS_W)

#define BOTTOM5_DIGITS 5
#define BOTTOM5_W (BOTTOM5_DIGITS * LED_DIGIT_W + (BOTTOM5_DIGITS - 1) * LED_FIELD_GAP)

/* 三字段行的几何。总宽 3*48 + 2*16 = 176，居中 → x = 32..207；
 * 数值行 y = 173..192，最低一行所需半径 hypot(87.5, 72.5) = 113.6。 */
#define FIELD_ROW_W  (3 * FIELD1_W + 2 * FIELD_GAP)
#define FIELD_X0     (CENTER_X - FIELD_ROW_W / 2)
#define FIELD_TOP    (CLOCK_Y + CLOCK_H + 20)
#define FIELD_LABEL_H 10
#define FIELD_VAL_Y  (FIELD_TOP + FIELD_LABEL_H + 3)

/* 倒数第二行：步数点阵居中，左右各一个状态图标位。
 * 图标 21px 高、点阵 20px 高，图标上移 1px 与点阵在视觉上对齐。
 * ICON_GAP 是往里收的间隙 —— 这一行已经很靠下，间隙越大图标越往外、
 * 越容易顶到表圈，所以只留 2px。 */
#define BOTTOM_ROW_Y (FIELD_VAL_Y + LED_DIGIT_H + 5)
#define BOTTOM5_X    (CENTER_X - BOTTOM5_W / 2)
#define ICON_W       22   /* 最宽的字形 'A' 是 21px，adv_w 19px，居中留 1px 余量 */
#define ICON_H       21
#define ICON_GAP     2
#define ICON_Y       (BOTTOM_ROW_Y - 1)
#define ICON_X_LEFT  (BOTTOM5_X - ICON_GAP - ICON_W)
#define ICON_X_RIGHT (BOTTOM5_X + BOTTOM5_W + ICON_GAP)

/* ---------------------------------------------------------------------------
 * 圆形可视区的编译期校核
 *
 * 全部按 2 倍坐标算，避开圆心 (119.5, 119.5) 的半像素：2*119.5 = 239。
 * 一个宽 w、纵向 y0..y1 的居中矩形，四角里最远的那个满足
 *     w² + max(|2*y0-239|, |2*y1-239|)² ≤ (2*SAFE_R)²
 * 就在安全圆内。改了列宽、CLOCK_Y 或字段行位置而越界的话，这里直接编译不过。
 * ------------------------------------------------------------------------- */
#define DY2(y)          ((2 * (y) - 239) < 0 ? (239 - 2 * (y)) : (2 * (y) - 239))
#define DY2_MAX(y0, y1) (DY2(y0) > DY2(y1) ? DY2(y0) : DY2(y1))
#define FITS_IN_SAFE_CIRCLE(w, y0, y1) \
    ((w) * (w) + DY2_MAX(y0, y1) * DY2_MAX(y0, y1) <= (2 * SAFE_R) * (2 * SAFE_R))

/* 三字段行的 x 布局只用了 FIELD1_W 一个宽度，三个字段必须等宽 */
BUILD_ASSERT(FIELD1_W == FIELD2_W && FIELD2_W == FIELD3_W,
             "data fields must be equal width: FIELD1/2/3_DIGITS must match");
BUILD_ASSERT(FITS_IN_SAFE_CIRCLE(CLOCK_W, CLOCK_Y, CLOCK_Y + CLOCK_H - 1),
             "clock block corners fall outside the round display; "
             "narrow COL_W/COLON_W/COL_GAP or move CLOCK_Y toward the center");
BUILD_ASSERT(FITS_IN_SAFE_CIRCLE(FIELD_ROW_W, FIELD_VAL_Y, FIELD_VAL_Y + LED_DIGIT_H - 1),
             "data field row corners fall outside the round display; "
             "reduce FIELD*_DIGITS or FIELD_GAP");
/* 图标位在最靠下的一行，外侧下角是整块表盘最容易被切的地方之一 */
BUILD_ASSERT(FITS_IN_SAFE_CIRCLE(2 * (CENTER_X - ICON_X_LEFT), ICON_Y, ICON_Y + ICON_H - 1),
             "left status icon falls outside the round display; reduce ICON_GAP/ICON_W");
BUILD_ASSERT(FITS_IN_SAFE_CIRCLE(2 * (ICON_X_RIGHT + ICON_W - CENTER_X), ICON_Y, ICON_Y + ICON_H - 1),
             "right status icon falls outside the round display; reduce ICON_GAP/ICON_W");

#define LED_BG_COLOR  ((lv_color_t)LV_COLOR_MAKE(0x08, 0x30, 0x39))
#define LED_FG_COLOR  ((lv_color_t)LV_COLOR_MAKE(0xff, 0xff, 0xff))

static int sim_steps = 8542;
static int sim_temp = 59;
static int sim_temp_hi = 63;
static int sim_humidity = 27;
static int sim_wind_mps = 4;         /* 风速，m/s */
static int sim_wind_bearing = 225;   /* 风向：风"从"哪个方位来，正北起顺时针度数 */
static int sim_precip = 40;          /* 降水概率 % */
static int sim_recovery = 5;
static int sim_last_hr = 80;
static int sim_week_min = 0;
static int sim_stress = 45;
static int sim_bodybatt = 68;
static int sim_battery = 85;

/* 状态图标的输入。真机上这些应该来自 RTC 闹钟表、BLE 连接回调和活动监测，
 * 目前没有对应子系统，先给一组默认值，改由 watchface_set_*() 从外部驱动。 */
static int  state_alarm_count = 1;
static bool state_dnd = false;
static bool state_phone_connected = true;
static int  state_move_bar = 0;          /* 0-5 */

/* 两个图标位显示什么。参考实现的默认是 icon1=闹钟、icon2=仅断开时显示蓝牙；
 * 这里右边用"连/断都显示"，否则手机连着的时候右边一直是空的，看不出这套
 * 逻辑在工作。要跟参考完全一致就把右边设成 ICON_SLOT_BLUETOOTH_OFF。 */
static icon_slot_t icon_slot_left  = ICON_SLOT_ALARM;
static icon_slot_t icon_slot_right = ICON_SLOT_BLUETOOTH;

static int sunrise_hour = 1, sunrise_min = 18;
static int sunset_hour = 3, sunset_min = 13;

/* ---------------------------------------------------------------------------
 * 重绘抑制
 *
 * lv_label_set_text() 不管内容有没有变，都会 lv_free + lv_malloc 一份文本、
 * 重新排版并 invalidate 整个 label —— 在 GC9A01 上就是一次真实的 SPI 刷屏。
 * 表盘上绝大多数内容一分钟、一天甚至更久才变一次，所以统一走下面两个 helper：
 * 值没变就什么都不做。同理 lv_obj_set_style_text_color() 也不做相等判断。
 * ------------------------------------------------------------------------- */
static void label_set_text(lv_obj_t *label, const char *text)
{
    if (!label) return;
    const char *cur = lv_label_get_text(label);
    if (cur && strcmp(cur, text) == 0) return;
    lv_label_set_text(label, text);
}

static void label_set_color(lv_obj_t *obj, lv_color_t color)
{
    if (!obj) return;
    if (lv_color_eq(lv_obj_get_style_text_color(obj, LV_PART_MAIN), color)) return;
    lv_obj_set_style_text_color(obj, color, LV_PART_MAIN);
}

/* ---------------------------------------------------------------------------
 * 分级缓存
 *
 * 日期 / 农历 / 节气 / 月相 / 日出日落：一天只变一次 —— 用 day_key 拦住。
 * 天气 + 气温：真实数据源（气象服务）没必要频繁取 —— WEATHER_REFRESH_MIN
 *              分钟才刷一次数据；渲染仍每轮调用，但被上面的 helper 挡掉。
 * 农历换算要线性扫 383 项月表 + 744 项节气表，务必只在换天时做一次。
 * ------------------------------------------------------------------------- */
#define WEATHER_REFRESH_MIN 30
#define DAY_KEY(tm_) (((tm_).tm_year + 1900) * 512 + (tm_).tm_yday)
#define DAY_KEY_NONE (-1)

static int cached_day_key = DAY_KEY_NONE;     /* 日期/农历/月相 已渲染到哪一天 */
static int lunar_day_key  = DAY_KEY_NONE;     /* 农历换算结果对应哪一天 */
static char lunar_line[64];                   /* 农历行缓存 */
static int64_t weather_next_refresh = 0;      /* 下次允许刷新天气数据的时间戳 */

/* 让所有缓存失效：切换语言/主题后必须整屏重算一次。 */
static void watchface_invalidate_cache(void)
{
    cached_day_key = DAY_KEY_NONE;
    lunar_day_key  = DAY_KEY_NONE;
    weather_next_refresh = 0;
}

/* 图标位 → 字形。同 Segment34 的 getIconState()：一个位置选一种指示器，
 * 画哪个字形由状态决定，条件不满足就返回空串（label 什么都不画）。
 * 字形出自 lv_font_icons：A=闹钟 D=勿扰 L=蓝牙 N..R=久坐提醒 1-5 级。
 *
 * 蓝牙断开在参考实现里是字形 'M'，但那个字形和 'L' 逐像素完全相同，区别只在
 * 整个符文画成 85/255 的灰度 —— 也就是"同一个图标，暗一档"。1bpp 字体里表达
 * 不了灰度，所以 'M' 干脆不收进字体（收了就是个空白字形，见 gen_font.py 的
 * 告警），断开状态改成同一个 'L' 配暗色，最终显示效果与参考一致。 */
static const char *icon_slot_glyph(icon_slot_t slot)
{
    switch (slot) {
    case ICON_SLOT_ALARM:
        return (state_alarm_count > 0) ? "A" : "";
    case ICON_SLOT_DND:
        return state_dnd ? "D" : "";
    case ICON_SLOT_BLUETOOTH:
        return "L";
    case ICON_SLOT_BLUETOOTH_OFF:
        return state_phone_connected ? "" : "L";
    case ICON_SLOT_MOVE_BAR:
        if (state_move_bar <= 0) return "";
        if (state_move_bar >= 5) return "R";
        return (const char *[]){ "N", "O", "P", "Q" }[state_move_bar - 1];
    case ICON_SLOT_NONE:
    default:
        return "";
    }
}

/* 只有蓝牙断开要画暗色，其余状态都是常规亮度 */
static bool icon_slot_dimmed(icon_slot_t slot)
{
    return (slot == ICON_SLOT_BLUETOOTH || slot == ICON_SLOT_BLUETOOTH_OFF)
           && !state_phone_connected;
}

/* 风向箭头。同 Segment34 的 getWind()：
 *     bearing = ((Math.round((windBearing + 180) / 45.0) % 8) + 97).toChar()
 * 把风向量化成 8 个方位，映射到它 LED 字体里 'a'-'h' 这 8 个箭头字形。
 * windBearing 是"风从哪个方位吹来"，+180 换成"吹向哪" —— 所以箭头指的是
 * 风去的方向，不是来的方向。这里用等价的整数运算（round(x/45) 写成
 * floor((2x+45)/90)），字形换成 Unicode 箭头，已加进 FONT_CJK。 */
static const char *wind_arrow(int bearing_deg)
{
    static const char *const arrows[8] = {
        "↑", "↗", "→", "↘", "↓", "↙", "←", "↖"   /* N NE E SE S SW W NW */
    };
    int b = ((bearing_deg % 360) + 360) % 360;
    return arrows[((2 * (b + 180) + 45) / 90) % 8];
}

/* 模拟数据是华氏度；界面统一按摄氏度显示。 */
static int fahrenheit_to_celsius(int f)
{
    int n = (f - 32) * 5;
    return (n >= 0) ? (n + 4) / 9 : (n - 4) / 9;   /* 补偿 C 的向零截断，做四舍五入 */
}

static void sim_update_data(void)
{
    sim_steps += (rand() % 5) + 1;
    sim_recovery = 4 + (rand() % 4);
    sim_last_hr = 75 + (rand() % 15);
    sim_stress = 30 + (rand() % 50);
    sim_bodybatt = 40 + (rand() % 50);
    sim_battery = 15 + (rand() % 85);
}

/* 天气数据刷新——真实设备上这里换成气象服务/BLE 取数，同样受间隔保护。 */
static void sim_update_weather(void)
{
    int64_t now = k_uptime_get();
    if (now < weather_next_refresh) return;
    weather_next_refresh = now + (int64_t)WEATHER_REFRESH_MIN * 60 * 1000;

    sim_temp = 55 + (rand() % 10);
    sim_temp_hi = sim_temp + 2 + (rand() % 6);
    sim_humidity = 20 + (rand() % 50);
    sim_wind_mps = rand() % 13;
    sim_wind_bearing = rand() % 360;
    sim_precip = rand() % 101;

    /* 状态量也跟着模拟，好看出图标逻辑在动：闹钟三分之二概率有，勿扰偶尔开，
     * 手机四分之一概率断连，久坐等级 0-5 —— 三种指示器都会走到空/非空两边。 */
    state_alarm_count = (rand() % 3) > 0 ? 1 : 0;
    state_dnd = (rand() % 5) == 0;
    state_phone_connected = (rand() % 4) != 0;
    state_move_bar = rand() % 6;
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

static lv_obj_t *icon_label_left = NULL;
static lv_obj_t *icon_label_right = NULL;

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

/* 列宽/列偏移见文件头 COL_* —— 冒号列比数字列窄，为了收进圆形可视区。
 * 窄列里的 42px 字形（'#' 网格、':'）走 CENTER + LV_LABEL_LONG_CLIP，
 * 等于居中裁切：网格是均匀重复的，冒号两点也在正中，裁掉的都是空边。 */

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

/* 状态图标位。内容由 watchface_update_icons() 填，空串就是什么都不画。
 * 字形 adv_w 19px 但最宽的 'A' 位图有 21px，居中对齐后正好落在 ICON_W=22
 * 的框里；CLIP 只是兜底，正常不会截到东西。 */
static lv_obj_t *icon_label_create(lv_obj_t *parent, int x, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, FONT_ICONS, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(label, 0, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(label, x, ICON_Y);
    lv_obj_set_size(label, ICON_W, ICON_H);
    lv_label_set_text(label, "");
    return label;
}

static void led_field_set_value(lv_obj_t **bg_labels, lv_obj_t **val_labels,
                                 int digits, float value, int decimals)
{
    char src_buf[8];
    int total_chars;

    if (!bg_labels || !val_labels) return;

    if (decimals == 1) {
        snprintf(src_buf, sizeof(src_buf), "%.1f", (double)value);
        /* 格数不够就先丢小数（12.5 → "12"）。不这么做的话下面的右对齐
         * 会把最高位截掉，"12.5" 在 3 格里显示成 "2.5" —— 错得看不出来。 */
        if ((int)strlen(src_buf) > digits) {
            snprintf(src_buf, sizeof(src_buf), "%d", (int)value);
        }
    } else {
        snprintf(src_buf, sizeof(src_buf), "%d", (int)value);
    }
    if ((int)strlen(src_buf) > digits) {
        memset(src_buf, '9', (size_t)digits);   /* 整数位都放不下：显示满量程 */
        src_buf[digits] = '\0';
    }
    total_chars = strlen(src_buf);

    /* 每格由两层组成，只有颜色和 val 的字符随取值变化：
     *   空位   bg='#' 暗绿（点阵底纹）          val=' '  —— 什么都不显示
     *   小数点 bg='#' 暗绿                      val='.'  白（'.' 字形是正常极性）
     *   数字   bg='#' 白（35 个白点做底）        val=数字 暗绿
     *          —— LED 字体的数字字形是反相的（不透明=非笔段），所以 val 用暗绿
     *             盖住非笔段，笔段透出底层的白，最终得到白字 + 暗绿底。
     * bg 的文本恒为 "#"，配合 label_set_text() 的相等判断，创建之后再不会重绘。
     *
     * NOTE: val 必须传单字符 + '\0'，不能传 &src_buf[src_idx]；否则 label 会拿到
     * 整个剩余字符串，居中 + LV_LABEL_LONG_CLIP 会显示中间那个字符
     * （"8698" 显示成 '6'/'9'），导致每个字段的数字都是错的。 */
    for (int i = 0; i < digits; i++) {
        int src_idx = i - (digits - total_chars);
        bool is_digit = (src_idx >= 0 && src_idx < total_chars && src_buf[src_idx] != '.');
        char val_text[2] = { ' ', '\0' };

        if (src_idx >= 0 && src_idx < total_chars) {
            val_text[0] = src_buf[src_idx];
        }

        label_set_text(bg_labels[i], "#");
        label_set_color(bg_labels[i], is_digit ? LED_FG_COLOR : LED_BG_COLOR);
        label_set_text(val_labels[i], val_text);
        label_set_color(val_labels[i], is_digit ? LED_BG_COLOR : LED_FG_COLOR);
    }
}

static const char *get_moon_string(int phase)
{
    /* 文字模式月相名称固定用英文：FONT_MOON 是 montserrat_8，不含汉字，
     * 中文模式下走 locale 会拿到中文字符串并显示成方框。
     * 4 个标签对应 8 个阶段，每 2 个阶段归一组。 */
    static const char * const moon_en[] = {
        "NEW",   /* phase 0 */
        "NEW",   /* phase 1 */
        "1QTR",  /* phase 2 */
        "1QTR",  /* phase 3 */
        "FULL",  /* phase 4 */
        "FULL",  /* phase 5 */
        "3QTR",  /* phase 6 */
        "3QTR",  /* phase 7 */
    };
    if (phase >= 0 && phase < 8) {
        return moon_en[phase];
    }
    return "";
}

/* 儒略日序数（Fliegel–Van Flandern），与 Segment34.CN 的 julianDay() 一致。 */
static int32_t julian_day(int year, int month, int day)
{
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    return day + (153 * m + 2) / 5 + 365L * y + y / 4 - y / 100 + y / 400 - 32045;
}

/*
 * 月相 0-7：0=新月 1=蛾眉月 2=上弦 3=盈凸 4=满月 5=亏凸 6=下弦 7=残月，
 * 与 lv_font_moon 的字形 '0'-'7' 一一对应。
 *
 * 算法照搬 Segment34.CN 的 moonPhase()：以 2023-01-21（JDN 2459966，一次朔）
 * 为原点求月龄，再按它那套非均匀区间（3/3/4/4/4/4/4/3 天）分桶——新月和满月
 * 的窗口比其它相位窄，这是参考实现刻意的取舍。
 *
 * 两处相对参考实现的改进（都不改变分桶规则）：
 *   1. 全整数运算，朔望月按 29.5306 天放大 10000 倍，避免浮点；
 *      2056 年时中间量约 1.24e8，离 int32 上限还差一个数量级。
 *   2. 原点偏移半天（-5000），因为 JDN 是正午换日而这里按当天正午取样。
 * 对 2026-2056 共 11315 天与天文月相比对：原实现（Conway 近似）偏差 16.0%，
 * 参考实现原样 12.2%，本实现 0.6%。
 */
#define MOON_EPOCH_JDN   2459966L    /* 2023-01-21，朔 */
#define MOON_CYCLE_X10K  295306L     /* 朔望月 29.5306 天 */

static int get_moon_phase(int year, int month, int day)
{
    int32_t days = julian_day(year, month, day) - MOON_EPOCH_JDN;
    int32_t age = (days * 10000 - 5000) % MOON_CYCLE_X10K;
    if (age < 0) age += MOON_CYCLE_X10K;

    if (age <  30000) return 0;
    if (age <  60000) return 1;
    if (age < 100000) return 2;
    if (age < 140000) return 3;
    if (age < 180000) return 4;
    if (age < 220000) return 5;
    if (age < 260000) return 6;
    if (age < 290000) return 7;
    return 0;
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
    sim_update_weather();   /* 内部按 WEATHER_REFRESH_MIN 限流 */
    watchface_update_sensors();
    ARG_UNUSED(timer);
}

/* 长按表盘触发设置面板 */
static void watchface_long_press_cb(lv_event_t *e)
{
    ARG_UNUSED(e);
    settings_show();
}

/* 短按（< 3s）：若设置面板可见则关闭 */
static void watchface_click_cb(lv_event_t *e)
{
    ARG_UNUSED(e);
    if (settings_is_visible()) {
        settings_hide();
    }
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
    lv_obj_set_pos(dawn_label, 64, 11);
    lv_obj_set_width(dawn_label, 40);

    dawn_time_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(dawn_time_label, FONT_TIME_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(dawn_time_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_align(dawn_time_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dawn_time_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(dawn_time_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(dawn_time_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dawn_time_label, 0, LV_PART_MAIN);
    lv_obj_set_pos(dawn_time_label, 64, 19);
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
    lv_obj_set_pos(moon_label, CENTER_X - 10, MOON_Y);
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
    lv_obj_set_pos(dusk_label, 136, 11);
    lv_obj_set_width(dusk_label, 40);

    dusk_time_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(dusk_time_label, FONT_TIME_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(dusk_time_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_align(dusk_time_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dusk_time_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(dusk_time_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(dusk_time_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dusk_time_label, 0, LV_PART_MAIN);
    lv_obj_set_pos(dusk_time_label, 136, 19);
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

    /* 4 个 42px 数字列 + 中间 18px 冒号列，总宽 COL_TOTAL 即 CLOCK_W */
    clock_col_h1 = clock_col_create(clock_bg, COL_X_H1, COL_W,
                     colors, "#", &clock_lbl_digit_h1, &clock_lbl_grid_h1);
    clock_col_h2 = clock_col_create(clock_bg, COL_X_H2, COL_W,
                     colors, "#", &clock_lbl_digit_h2, &clock_lbl_grid_h2);
    clock_col_colon = clock_col_create(clock_bg, COL_X_COLON, COLON_W,
                     colors, "#", &clock_lbl_digit_colon, &clock_lbl_grid_colon);
    clock_col_m1 = clock_col_create(clock_bg, COL_X_M1, COL_W,
                     colors, "#", &clock_lbl_digit_m1, &clock_lbl_grid_m1);
    clock_col_m2 = clock_col_create(clock_bg, COL_X_M2, COL_W,
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
    lv_obj_set_pos(stress_bar, CLOCK_X - 4, CLOCK_Y + CLOCK_H - stress_h);
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
    lv_obj_set_pos(bodybatt_bar, CLOCK_X + CLOCK_W + 1, CLOCK_Y + CLOCK_H - bodybatt_h);
    lv_obj_set_style_bg_color(bodybatt_bar, colors->bodybatt, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bodybatt_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bodybatt_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bodybatt_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bodybatt_bar, 0, LV_PART_MAIN);
    printk("watchface_start: body batt bar created\n");

    /* Date line + seconds
     * Row 6:      MON, 5 MAY 2025      32
     * 日期两侧各预留 SECONDS_W（与右侧秒数标签等宽），在剩余区域内居中，
     * 因此日期的中心正好落在表盘中心，中英文一致。
     * 日期行也使用实体线字体 FONT_DATA = montserrat_12 */
    date_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(date_label, FONT_DATA, LV_PART_MAIN);
    lv_obj_set_style_text_color(date_label, colors->text, LV_PART_MAIN);
    lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(date_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(date_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(date_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(date_label, 0, LV_PART_MAIN);
    lv_label_set_long_mode(date_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(date_label, CLOCK_X + SECONDS_W, CLOCK_Y + CLOCK_H + 4);
    lv_obj_set_width(date_label, DATE_W);

    seconds_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(seconds_label, FONT_DATA, LV_PART_MAIN);
    lv_obj_set_style_text_color(seconds_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_align(seconds_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(seconds_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(seconds_label, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(seconds_label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(seconds_label, 0, LV_PART_MAIN);
    lv_obj_set_pos(seconds_label, CLOCK_X + CLOCK_W - SECONDS_W, CLOCK_Y + CLOCK_H + 4);
    lv_obj_set_width(seconds_label, SECONDS_W);

    /* Three data fields: label on top (solid font), LED dot-matrix value below
     * Row 7: RECOVERY HRS:   LAST HR:   WEEK ACT MIN:   （标签 montserrat_8）
     * Row 8:     5.0           80           0             （数值 LED 点阵风格） */
    int field_top = FIELD_TOP;
    int field_h = LED_DIGIT_H;
    int field1_x = FIELD_X0;
    int field2_x = FIELD_X0 + FIELD1_W + FIELD_GAP;
    int field3_x = FIELD_X0 + 2 * (FIELD1_W + FIELD_GAP);

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

    led_field_create(root_page, field1_x, FIELD_VAL_Y, FIELD1_W, field_h, FIELD1_DIGITS, field1_bg_labels, field1_val_labels);

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

    led_field_create(root_page, field2_x, FIELD_VAL_Y, FIELD2_W, field_h, FIELD2_DIGITS, field2_bg_labels, field2_val_labels);

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

    led_field_create(root_page, field3_x, FIELD_VAL_Y, FIELD3_W, field_h, FIELD3_DIGITS, field3_bg_labels, field3_val_labels);
    printk("watchface_start: fields created\n");

    /* 倒数第二行：状态图标 + 5 位步数点阵 + 状态图标
     * Row 9:  ⏰   0 8 5 7 3   ᛒ
     *
     * 原来这两侧画的是心形和火焰，用的是 icons.c 里那套自绘位图 —— 它把
     * 局部坐标当层坐标传给 lv_draw_rect()，像素全落在屏幕左上角、又被裁到
     * 对象自己的范围里，所以从来一个点都没画出来过。整套换成图标字体：
     * 走 label + label_set_text()，重绘抑制、换色、裁剪全是现成的。
     * 心率本来就在正上方的 LAST HR 字段里，这里让给状态指示更有用。 */
    led_field_create(root_page, BOTTOM5_X, BOTTOM_ROW_Y, BOTTOM5_W, LED_DIGIT_H,
                     BOTTOM5_DIGITS, bottom5_bg_labels, bottom5_val_labels);

    icon_label_left  = icon_label_create(root_page, ICON_X_LEFT, colors->notif);
    icon_label_right = icon_label_create(root_page, ICON_X_RIGHT, colors->notif);
    printk("watchface_start: bottom row created\n");

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

    /* 透明输入捕获层：全屏覆盖表盘内容，专门接收长按/点击事件。
     *
     * 为什么不直接在 root_page 上注册：root_page 的子对象（clock_bg、bar 等
     * 由 lv_obj_create 生成）默认同时带有 LV_OBJ_FLAG_CLICKABLE 和
     * LV_OBJ_FLAG_SCROLLABLE，LVGL 把事件派发给最上层命中的子对象后不会
     * 自动冒泡到 root_page，导致长按永远到不了 root_page。
     *
     * 透明层叠在所有表盘内容之上（z-order 最高），自己没有可滚动子对象，
     * 因此 LV_EVENT_LONG_PRESSED 能可靠触发。settings_init() 在此之后调用，
     * 面板对象的 z-order 更高，点击面板按钮时不会被本层拦截。
     *
     * native_sim：鼠标按住不动约 400ms 即触发。
     * 真机（无触摸屏）：在 main.c 的 GPIO 回调里直接调 settings_show()，
     *                    不依赖这个透明层。 */
    lv_obj_t *input_layer = lv_obj_create(lv_scr_act());
    lv_obj_set_size(input_layer, SCREEN_W, SCREEN_H);
    lv_obj_center(input_layer);
    lv_obj_set_style_bg_opa(input_layer,     LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(input_layer, 0,           LV_PART_MAIN);
    lv_obj_set_style_pad_all(input_layer,    0,             LV_PART_MAIN);
    lv_obj_set_style_radius(input_layer,     0,             LV_PART_MAIN);
    lv_obj_clear_flag(input_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(input_layer,   LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(input_layer, watchface_long_press_cb, LV_EVENT_LONG_PRESSED,  NULL);
    lv_obj_add_event_cb(input_layer, watchface_click_cb,      LV_EVENT_SHORT_CLICKED, NULL);

    /* 长按阈值改为 5000ms：遍历所有已注册的输入设备统一设置。
     * LVGL 的指针设备由 Zephyr LVGL 模块在 SYS_INIT 里创建，
     * 这里 watchface_start() 在 main() 里调用，设备已就绪。 */
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev != NULL) {
        lv_indev_set_long_press_time(indev, 3000);
        indev = lv_indev_get_next(indev);
    }

    settings_init();
    printk("watchface_start: settings initialized\n");
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

    /* 每格 42x80，四个数字每秒重写一次就是整块时钟每秒重绘一次。
     * label_set_text() 让它们只在真正跳字时才重绘（分位每分钟、时位每小时）。 */
    char dig[2];
    dig[0] = '0' + (timeinfo.tm_hour / 10);
    dig[1] = '\0';
    label_set_text(clock_lbl_digit_h1, dig);
    dig[0] = '0' + (timeinfo.tm_hour % 10);
    label_set_text(clock_lbl_digit_h2, dig);
    dig[0] = '0' + (timeinfo.tm_min / 10);
    label_set_text(clock_lbl_digit_m1, dig);
    dig[0] = '0' + (timeinfo.tm_min % 10);
    label_set_text(clock_lbl_digit_m2, dig);

    /* Colon always visible — dot areas transparent showing yellow,
     * non-dot areas dark green. The black grid layer stays always visible. */
    label_set_text(clock_lbl_digit_colon, ":");
}

void watchface_update_date(void)
{
    if (!date_label || !seconds_label || !dawn_label || !dusk_label || 
        !dawn_time_label || !dusk_time_label || !moon_label) return;

    struct tm timeinfo;
    time_t now = time(NULL);
    localtime_r(&now, &timeinfo);

    /* 秒是这一行里唯一每秒都变的东西，先单独更新掉。 */
    char sec_str[8];
    snprintf(sec_str, sizeof(sec_str), "%02d", timeinfo.tm_sec);
    label_set_text(seconds_label, sec_str);

    /* 其余（日期串、日出日落、月相）一天只变一次，换天之前直接返回。 */
    int day_key = DAY_KEY(timeinfo);
    if (day_key == cached_day_key) return;
    cached_day_key = day_key;

    /* 日期行：ZH 用 "周X yyyy - MM - dd"，EN 用 "MON, 5 MAY 2025" */
    char date_str[48];
    if (current_lang == LANG_ZH) {
        static const char *zhou[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
        snprintf(date_str, sizeof(date_str), "%s %04d - %02d - %02d",
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
    label_set_text(date_label, date_str);

    /* DAWN/DUSK 始终英文 */
    label_set_text(dawn_label, "DAWN:");
    label_set_text(dusk_label, "DUSK:");

    char dawn_time_str[16];
    char dusk_time_str[16];
    snprintf(dawn_time_str, sizeof(dawn_time_str), "%02d:%02d", sunrise_hour, sunrise_min);
    snprintf(dusk_time_str, sizeof(dusk_time_str), "%02d:%02d", sunset_hour, sunset_min);
    label_set_text(dawn_time_label, dawn_time_str);
    label_set_text(dusk_time_label, dusk_time_str);

    /* 月相显示：0=文字，1=图片（默认）。lv_font_moon 的字形 '0'-'7' 依次是
     * 新月/蛾眉/上弦/盈凸/满月/亏凸/下弦/残月，与 get_moon_phase() 的返回一一对应。 */
    int moon_phase = get_moon_phase(timeinfo.tm_year + 1900,
                                    timeinfo.tm_mon + 1, timeinfo.tm_mday);
    if (moon_display_mode == 0) {
        lv_obj_set_style_text_font(moon_label, FONT_MOON, LV_PART_MAIN);
        lv_obj_set_width(moon_label, 32);
        lv_obj_set_pos(moon_label, CENTER_X - 16, MOON_Y + 6);
        label_set_text(moon_label, get_moon_string(moon_phase));
    } else {
        /* 5 月 4 日（May the Fourth be with you）画死星，字形 '8'。
         * 同 Segment34.CN moonPhase() 里的彩蛋：
         *     if(time.month == 5 and time.day == 4) { return "8"; // That's no moon!
         * 只在图片模式生效——文字模式没有对应的词，仍显示当天真实月相。 */
        bool death_star = (timeinfo.tm_mon + 1 == 5 && timeinfo.tm_mday == 4);

        lv_obj_set_style_text_font(moon_label, FONT_MOON_IMAGE, LV_PART_MAIN);
        lv_obj_set_width(moon_label, 20);
        lv_obj_set_pos(moon_label, CENTER_X - 10, MOON_Y);
        char moon_char[2] = { (char)('0' + (death_star ? 8 : moon_phase)), '\0' };
        label_set_text(moon_label, moon_char);
    }
}

/*
 * 农历行 "丙午年 六月廿三 +2立秋"。
 * lunar_calendar_convert() 要线性扫 383 项月表 + 744 项节气表，而结果一天只
 * 变一次 —— 按天缓存，换天之前直接返回上次的字符串。
 */
static const char *lunar_line_for_today(const struct tm *t)
{
    int day_key = DAY_KEY(*t);
    if (day_key == lunar_day_key) return lunar_line;

    lunar_date_t lunar;
    lunar_calendar_convert(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, &lunar);
    if (lunar.jieqi[0] != '\0') {
        snprintf(lunar_line, sizeof(lunar_line), "%s %s%s %s",
                 lunar.year_name, lunar.month_name, lunar.day_name, lunar.jieqi);
    } else {
        snprintf(lunar_line, sizeof(lunar_line), "%s %s%s",
                 lunar.year_name, lunar.month_name, lunar.day_name);
    }
    lunar_day_key = day_key;
    LOG_INF("[LUNAR] %s (year=%d month=%d day=%d leap=%d)",
            lunar_line, lunar.year, lunar.month, lunar.day, lunar.leap_month);
    return lunar_line;
}

void watchface_update_weather(void)
{
    if (!temp_label || !weather_label) return;

    /* ℃(U+2103) 不在 montserrat_12 里，会画成空心方框；FONT_CJK 含完整 ASCII
     * 且行高/基线与 montserrat_12 相同，中英文这一行都用它，布局不受影响。 */
    lv_obj_set_style_text_font(temp_label, FONT_CJK, LV_PART_MAIN);

    char temp_str[64];
    int lo = fahrenheit_to_celsius(sim_temp);
    int hi = fahrenheit_to_celsius(sim_temp_hi);

    /* 天气行沿用 Segment34 的复合字段拼法（joinFour，", " 分隔）：
     *     温度, 风向箭头+风速, 湿度
     * 风速取 m/s 且不带单位后缀 —— 参考实现 windUnit 默认就是 0(m/s)，
     * getWind() 返回的就是 bearing + windspeed，中间没有分隔也没有单位。
     *
     * 降水概率不当成 joinFour 的第四段，而是跟在天气描述后面的括号里。
     * 参考实现的 complicationType 63 会把湿度和降水概率并排成两个裸百分比
     * （"27%, 40%"），谁是谁完全看不出来；它自己的 getWeatherCondition(true)
     * 正是用 " (NN%)" 表示降水概率的，这里沿用那个写法。 */
    char precip_str[16] = "";
    if (sim_precip > 0) {   /* 同参考实现：概率为 0 时整段不显示 */
        snprintf(precip_str, sizeof(precip_str), " (%d%%)", sim_precip);
    }

    if (current_lang == LANG_ZH) {
        /* 第三行："多云 15~17℃, ↗4, 27%"
         *
         * 中文模式没有降水概率：第四行让给了农历，天气描述只能挤在这一行，
         * 再加 " (40%)" 就是 181px —— 这一行在圆心上方，最宽处受它的上边缘
         * (y=32) 约束，只有 155px 可用，两头都会压到表圈上。实测过。 */
        snprintf(temp_str, sizeof(temp_str), "%s %d~%d℃, %s%d, %d%%",
                 locale_get_string(LOCALE_STR_PARTLY_CLOUDY), lo, hi,
                 wind_arrow(sim_wind_bearing), sim_wind_mps, sim_humidity);
        label_set_text(temp_label, temp_str);

        /* 第四行：干支年 + 农历月日 + 节气 */
        struct tm timeinfo;
        time_t now = time(NULL);
        localtime_r(&now, &timeinfo);
        lv_obj_set_style_text_font(weather_label, FONT_CJK, LV_PART_MAIN);
        label_set_text(weather_label, lunar_line_for_today(&timeinfo));
    } else {
        /* 第三行 "15~17℃, ↗4, 27%"，第四行 "PARTLY CLOUDY (40%)"。
         * 英文模式第四行就是天气描述，降水概率跟在那儿，第三行省下宽度。 */
        snprintf(temp_str, sizeof(temp_str), "%d~%d℃, %s%d, %d%%",
                 lo, hi, wind_arrow(sim_wind_bearing), sim_wind_mps, sim_humidity);
        label_set_text(temp_label, temp_str);

        char cond_str[40];
        snprintf(cond_str, sizeof(cond_str), "PARTLY CLOUDY%s", precip_str);
        lv_obj_set_style_text_font(weather_label, FONT_MED, LV_PART_MAIN);
        label_set_text(weather_label, cond_str);
    }
}

void watchface_update_battery(void)
{
    if (!battery_fill) return;
    /* battery_level temporary */
    {
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
        if (battery_display_mode == 1) {
            /* Inside: number only, no % sign (limited space inside 24×12px battery) */
            char percent_str[4];
            snprintf(percent_str, sizeof(percent_str), "%d", battery_level);
            label_set_text(battery_label, percent_str);
            lv_obj_clear_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(battery_percent_label, LV_OBJ_FLAG_HIDDEN);
        } else if (battery_display_mode == 2) {
            /* Outside: number with % sign, red text on low battery (<= 20%) */
            char percent_str[5];
            snprintf(percent_str, sizeof(percent_str), "%d%%", battery_level);
            label_set_text(battery_percent_label, percent_str);

            lv_color_t text_color = (battery_level <= 20)
                ? (lv_color_t)LV_COLOR_MAKE(0xFF, 0x33, 0x33)  /* Red warning */
                : theme_get_colors()->data_val;                 /* Normal color */
            lv_obj_set_style_text_color(battery_percent_label, text_color, LV_PART_MAIN);

            lv_obj_clear_flag(battery_percent_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            /* Hidden */
            lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(battery_percent_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void watchface_update_sensors(void)
{
    if (!field1_label || !field2_label || !field3_label) return;
    if (!stress_bar || !bodybatt_bar) return;

    label_set_text(field1_label, "RECOVERY HRS:");
    led_field_set_value(field1_bg_labels, field1_val_labels, FIELD1_DIGITS, (float)sim_recovery + 0.0f, 1);

    label_set_text(field2_label, "LAST HR:");
    led_field_set_value(field2_bg_labels, field2_val_labels, FIELD2_DIGITS, (float)sim_last_hr, 0);

    label_set_text(field3_label, "WEEK ACT MIN:");
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
    watchface_update_icons();
}

void watchface_update_icons(void)
{
    if (!icon_label_left || !icon_label_right) return;

    const theme_colors_t *colors = theme_get_colors();
    /* 85/255 正是参考图集里"蓝牙断开"那个字形的灰度，照抄这个比例 */
    lv_color_t dim = lv_color_mix(colors->notif, colors->bg, 85);

    label_set_text(icon_label_left,  icon_slot_glyph(icon_slot_left));
    label_set_color(icon_label_left,
                    icon_slot_dimmed(icon_slot_left) ? dim : colors->notif);

    label_set_text(icon_label_right, icon_slot_glyph(icon_slot_right));
    label_set_color(icon_label_right,
                    icon_slot_dimmed(icon_slot_right) ? dim : colors->notif);
}

void watchface_set_icon_slots(icon_slot_t left, icon_slot_t right)
{
    icon_slot_left = left;
    icon_slot_right = right;
    watchface_update_icons();
}

void watchface_set_alarm_count(int count)
{
    state_alarm_count = count;
    watchface_update_icons();
}

void watchface_set_dnd(bool on)
{
    state_dnd = on;
    watchface_update_icons();
}

void watchface_set_phone_connected(bool connected)
{
    state_phone_connected = connected;
    watchface_update_icons();
}

void watchface_set_move_bar_level(int level)
{
    /* 越界的等级按最近的合法值收，别让它去索引字形表 */
    state_move_bar = level < 0 ? 0 : (level > 5 ? 5 : level);
    watchface_update_icons();
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
    watchface_invalidate_cache();
    watchface_update_date();
}

void watchface_switch_language(void)
{
    if (current_lang == LANG_ZH) {
        locale_set_current(LANG_EN);
    } else {
        locale_set_current(LANG_ZH);
    }
    watchface_invalidate_cache();
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

    watchface_update_icons();   /* 图标颜色分亮/暗两种，统一由它算 */
}
