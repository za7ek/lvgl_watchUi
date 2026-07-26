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
#define CLOCK_Y 80

#define FONT_LABEL  &lv_font_montserrat_8
#define FONT_DATA   &lv_font_montserrat_10
#define FONT_MED    &lv_font_montserrat_12
#define FONT_BIG    &lv_font_cjk_16

static int sim_steps = 8542;
static int sim_temp = 59;
static int sim_temp_hi = 63;
static int sim_humidity = 27;
static int sim_recovery = 5;
static int sim_last_hr = 80;
static int sim_week_min = 0;
static int sim_stress = 45;
static int sim_bodybatt = 68;

static int sunrise_hour = 1, sunrise_min = 18;
static int sunset_hour = 3, sunset_min = 13;

static void sim_update_data(void)
{
    sim_steps += (rand() % 5) + 1;
    sim_recovery = 4 + (rand() % 4);
    sim_last_hr = 75 + (rand() % 15);
    sim_stress = 30 + (rand() % 50);
    sim_bodybatt = 40 + (rand() % 50);
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
static lv_obj_t *clock_label = NULL;
static lv_obj_t *date_label = NULL;
static lv_obj_t *seconds_label = NULL;

static lv_obj_t *field1_label = NULL;
static lv_obj_t *field1_value = NULL;
static lv_obj_t *field2_label = NULL;
static lv_obj_t *field2_value = NULL;
static lv_obj_t *field3_label = NULL;
static lv_obj_t *field3_value = NULL;

static lv_obj_t *bottom5_label = NULL;

static lv_obj_t *stress_bar = NULL;
static lv_obj_t *bodybatt_bar = NULL;

static lv_timer_t *time_timer = NULL;
static lv_timer_t *sensor_timer = NULL;

/* Draw an 8-pixel grid texture on the clock background using the current
 * theme's clock_off color, so the clock area has a subtle technical texture. */
static void clock_draw_event_cb(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_obj_t *obj = lv_event_get_target(e);

    const theme_colors_t *colors = theme_get_colors();

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = colors->clock_off;
    dsc.bg_opa = LV_OPA_20;
    dsc.radius = 0;

    lv_coord_t w = lv_obj_get_width(obj);
    lv_coord_t h = lv_obj_get_height(obj);

    lv_area_t area;

    /* Vertical grid lines every 8 pixels */
    for (lv_coord_t x = 0; x <= w; x += 8) {
        area.x1 = x;
        area.x2 = x;
        area.y1 = 0;
        area.y2 = h;
        lv_draw_rect(layer, &dsc, &area);
    }

    /* Horizontal grid lines every 8 pixels */
    for (lv_coord_t y = 0; y <= h; y += 8) {
        area.x1 = 0;
        area.x2 = w;
        area.y1 = y;
        area.y2 = y;
        lv_draw_rect(layer, &dsc, &area);
    }
}

static const char *get_moon_string(int phase)
{
    static const locale_str_id_t moon_ids[] = {
        LOCALE_STR_MOON_NEW, LOCALE_STR_MOON_FIRST_Q,
        LOCALE_STR_MOON_FULL, LOCALE_STR_MOON_THIRD_Q
    };
    if (phase >= 0 && phase < 4) {
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
    return (r * 4) / 30;
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

    lv_obj_set_style_bg_color(lv_scr_act(), colors->bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(lv_scr_act(), 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(lv_scr_act(), 0, LV_PART_MAIN);

    root_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(root_page, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(root_page, colors->bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_page, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(root_page, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root_page, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(root_page, 0, LV_PART_MAIN);
    lv_obj_center(root_page);

    /* Top: DAWN label + time | moon | DUSK label + time
     * Row 1 (y=8):  DAWN: [moon_icon] DUSK:
     * Row 2 (y=22): 01:18  [1QTR]  03:13 */
    dawn_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(dawn_label, FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(dawn_label, colors->field_lbl, LV_PART_MAIN);
    lv_obj_set_style_text_align(dawn_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_pos(dawn_label, 20, 8);
    lv_obj_set_width(dawn_label, 65);

    dawn_time_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(dawn_time_label, FONT_DATA, LV_PART_MAIN);
    lv_obj_set_style_text_color(dawn_time_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_align(dawn_time_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_pos(dawn_time_label, 20, 22);
    lv_obj_set_width(dawn_time_label, 65);

    moon_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(moon_label, FONT_DATA, LV_PART_MAIN);
    lv_obj_set_style_text_color(moon_label, colors->moon, LV_PART_MAIN);
    lv_obj_set_style_text_align(moon_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(moon_label, CENTER_X - 20, 14);
    lv_obj_set_width(moon_label, 40);

    dusk_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(dusk_label, FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(dusk_label, colors->field_lbl, LV_PART_MAIN);
    lv_obj_set_style_text_align(dusk_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_pos(dusk_label, 155, 8);
    lv_obj_set_width(dusk_label, 65);

    dusk_time_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(dusk_time_label, FONT_DATA, LV_PART_MAIN);
    lv_obj_set_style_text_color(dusk_time_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_align(dusk_time_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_pos(dusk_time_label, 155, 22);
    lv_obj_set_width(dusk_time_label, 65);

    /* Weather: temp line + description line
     * Row 3 (y=40):  59F, ↑4, 27%
     * Row 4 (y=58):  PARTLY CLOUDY  */
    temp_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(temp_label, FONT_MED, LV_PART_MAIN);
    lv_obj_set_style_text_color(temp_label, colors->text, LV_PART_MAIN);
    lv_obj_set_style_text_align(temp_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(temp_label, SCREEN_W);
    lv_obj_set_pos(temp_label, 0, 40);

    weather_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(weather_label, FONT_MED, LV_PART_MAIN);
    lv_obj_set_style_text_color(weather_label, colors->weather, LV_PART_MAIN);
    lv_obj_set_style_text_align(weather_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(weather_label, SCREEN_W);
    lv_obj_set_pos(weather_label, 0, 58);

    /* Large clock — using segments80 bitmap font.
     * clock_bg provides a black backdrop with an 8-pixel grid texture drawn
     * from the theme's clock_off color; the clock digits render on top. */
    clock_bg = lv_obj_create(root_page);
    lv_obj_set_pos(clock_bg, CLOCK_X, CLOCK_Y);
    lv_obj_set_size(clock_bg, CLOCK_W, CLOCK_H);
    lv_obj_set_style_bg_color(clock_bg, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(clock_bg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(clock_bg, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(clock_bg, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(clock_bg, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(clock_bg, clock_draw_event_cb, LV_EVENT_DRAW_MAIN, NULL);

    clock_label = lv_label_create(clock_bg);
    lv_obj_set_style_text_font(clock_label, &lv_font_segments80, LV_PART_MAIN);
    lv_obj_set_style_text_color(clock_label, colors->clock_on, LV_PART_MAIN);
    lv_obj_set_style_text_align(clock_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(clock_label, 0, 0);
    lv_obj_set_size(clock_label, CLOCK_W, CLOCK_H);
    lv_label_set_long_mode(clock_label, LV_LABEL_LONG_CLIP);

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

    /* Date line + seconds
     * Row 6: MON, 5 MAY 2025        32 */
    date_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(date_label, FONT_DATA, LV_PART_MAIN);
    lv_obj_set_style_text_color(date_label, colors->text, LV_PART_MAIN);
    lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_pos(date_label, 15, CLOCK_Y + CLOCK_H + 8);

    seconds_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(seconds_label, FONT_DATA, LV_PART_MAIN);
    lv_obj_set_style_text_color(seconds_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_align(seconds_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_pos(seconds_label, 185, CLOCK_Y + CLOCK_H + 8);
    lv_obj_set_width(seconds_label, 40);

    /* Three data fields: label on top, value below
     * Row 7: RECOVERY HRS:   LAST HR:   WEEK ACT MIN:
     * Row 8:     5.0           80           0      */
    int field_top = CLOCK_Y + CLOCK_H + 20;
    int field_w = 72;

    field1_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(field1_label, FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(field1_label, colors->field_lbl, LV_PART_MAIN);
    lv_obj_set_style_text_align(field1_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(field1_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(field1_label, 12, field_top);
    lv_obj_set_width(field1_label, field_w);

    field1_value = lv_label_create(root_page);
    lv_obj_set_style_text_font(field1_value, FONT_BIG, LV_PART_MAIN);
    lv_obj_set_style_text_color(field1_value, colors->heart_rate, LV_PART_MAIN);
    lv_obj_set_style_text_align(field1_value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(field1_value, 12, field_top + 12);
    lv_obj_set_width(field1_value, field_w);

    field2_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(field2_label, FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(field2_label, colors->field_lbl, LV_PART_MAIN);
    lv_obj_set_style_text_align(field2_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(field2_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(field2_label, CENTER_X - field_w/2, field_top);
    lv_obj_set_width(field2_label, field_w);

    field2_value = lv_label_create(root_page);
    lv_obj_set_style_text_font(field2_value, FONT_BIG, LV_PART_MAIN);
    lv_obj_set_style_text_color(field2_value, colors->steps, LV_PART_MAIN);
    lv_obj_set_style_text_align(field2_value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(field2_value, CENTER_X - field_w/2, field_top + 12);
    lv_obj_set_width(field2_value, field_w);

    field3_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(field3_label, FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(field3_label, colors->field_lbl, LV_PART_MAIN);
    lv_obj_set_style_text_align(field3_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(field3_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(field3_label, 156, field_top);
    lv_obj_set_width(field3_label, field_w);

    field3_value = lv_label_create(root_page);
    lv_obj_set_style_text_font(field3_value, FONT_BIG, LV_PART_MAIN);
    lv_obj_set_style_text_color(field3_value, colors->accent, LV_PART_MAIN);
    lv_obj_set_style_text_align(field3_value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(field3_value, 156, field_top + 12);
    lv_obj_set_width(field3_value, field_w);

    /* Bottom: icon + 5-digit steps + icon
     * Row 9:  ♥   0 8 5 7 3   🔥   */
    int bottom_y = field_top + 36;
    icon_draw(root_page, ICON_HEART, 30, bottom_y + 2, colors->heart_rate);

    bottom5_label = lv_label_create(root_page);
    lv_obj_set_style_text_font(bottom5_label, FONT_MED, LV_PART_MAIN);
    lv_obj_set_style_text_color(bottom5_label, colors->clock_on, LV_PART_MAIN);
    lv_obj_set_style_text_align(bottom5_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(bottom5_label, CENTER_X - 40, bottom_y);
    lv_obj_set_width(bottom5_label, 80);

    icon_draw(root_page, ICON_CALORIES, 200, bottom_y + 2, colors->accent);

    /* Battery icon — pixel style
     * Row 10: centered battery icon */
    int battery_y = bottom_y + 16;
    icon_draw(root_page, ICON_BATTERY_FULL, CENTER_X - 4, battery_y, colors->battery);

    watchface_update_time();
    watchface_update_date();
    watchface_update_weather();
    watchface_update_sensors();

    time_timer = lv_timer_create(time_update_cb, 1000, NULL);
    sensor_timer = lv_timer_create(sensor_update_cb, 10000, NULL);
}

void watchface_stop(void)
{
    if (time_timer) { lv_timer_del(time_timer); time_timer = NULL; }
    if (sensor_timer) { lv_timer_del(sensor_timer); sensor_timer = NULL; }
    if (root_page) { lv_obj_del(root_page); root_page = NULL; }
}

void watchface_update_time(void)
{
    struct tm timeinfo;
    time_t now = time(NULL);
    localtime_r(&now, &timeinfo);

    char time_str[8];
    if (timeinfo.tm_sec % 2 == 0) {
        snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    } else {
        snprintf(time_str, sizeof(time_str), "%02d %02d", timeinfo.tm_hour, timeinfo.tm_min);
    }
    lv_label_set_text(clock_label, time_str);

    char sec_str[8];
    snprintf(sec_str, sizeof(sec_str), "%02d", timeinfo.tm_sec);
    lv_label_set_text(seconds_label, sec_str);
}

void watchface_update_date(void)
{
    struct tm timeinfo;
    time_t now = time(NULL);
    localtime_r(&now, &timeinfo);

    const char *weekday_str;
    if (timeinfo.tm_wday == 0) {
        weekday_str = locale_get_string(LOCALE_STR_SUNDAY);
    } else {
        weekday_str = locale_get_string(LOCALE_STR_MONDAY + timeinfo.tm_wday - 1);
    }

    char date_str[48];
    snprintf(date_str, sizeof(date_str), "%s, %d %s %d",
             weekday_str, timeinfo.tm_mday,
             locale_get_string(LOCALE_STR_JANUARY + timeinfo.tm_mon),
             timeinfo.tm_year + 1900);
    lv_label_set_text(date_label, date_str);

    int moon_phase = get_moon_phase(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);

    char dawn_str[24];
    char dusk_str[24];
    snprintf(dawn_str, sizeof(dawn_str), "%s:", locale_get_string(LOCALE_STR_SUNRISE));
    snprintf(dusk_str, sizeof(dusk_str), "%s:", locale_get_string(LOCALE_STR_SUNSET));
    lv_label_set_text(dawn_label, dawn_str);
    lv_label_set_text(dusk_label, dusk_str);

    char dawn_time_str[16];
    char dusk_time_str[16];
    snprintf(dawn_time_str, sizeof(dawn_time_str), "%02d:%02d", sunrise_hour, sunrise_min);
    snprintf(dusk_time_str, sizeof(dusk_time_str), "%02d:%02d", sunset_hour, sunset_min);
    lv_label_set_text(dawn_time_label, dawn_time_str);
    lv_label_set_text(dusk_time_label, dusk_time_str);

    lv_label_set_text(moon_label, get_moon_string(moon_phase));
}

void watchface_update_weather(void)
{
    if (!temp_label || !weather_label) return;

    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "%dF, +%d, %d%%",
             sim_temp, sim_temp_hi - sim_temp, sim_humidity);
    lv_label_set_text(temp_label, temp_str);

    lv_label_set_text(weather_label, "PARTLY CLOUDY");
}

void watchface_update_sensors(void)
{
    lv_label_set_text(field1_label, "RECOVERY HRS:");
    char f1_str[16];
    snprintf(f1_str, sizeof(f1_str), "%d.%d", sim_recovery, 0);
    lv_label_set_text(field1_value, f1_str);

    lv_label_set_text(field2_label, "LAST HR:");
    char f2_str[16];
    snprintf(f2_str, sizeof(f2_str), "%d", sim_last_hr);
    lv_label_set_text(field2_value, f2_str);

    lv_label_set_text(field3_label, "WEEK ACT MIN:");
    char f3_str[16];
    snprintf(f3_str, sizeof(f3_str), "%d", sim_week_min);
    lv_label_set_text(field3_value, f3_str);

    char steps_str[16];
    snprintf(steps_str, sizeof(steps_str), "%05d", sim_steps);
    lv_label_set_text(bottom5_label, steps_str);

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
    lv_obj_set_style_text_color(clock_label, colors->clock_on, LV_PART_MAIN);
    lv_obj_invalidate(clock_bg);

    lv_obj_set_style_text_color(dawn_label, colors->field_lbl, LV_PART_MAIN);
    lv_obj_set_style_text_color(dawn_time_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_color(dusk_label, colors->field_lbl, LV_PART_MAIN);
    lv_obj_set_style_text_color(dusk_time_label, colors->data_val, LV_PART_MAIN);
    lv_obj_set_style_text_color(moon_label, colors->moon, LV_PART_MAIN);
    lv_obj_set_style_text_color(temp_label, colors->text, LV_PART_MAIN);
    lv_obj_set_style_text_color(weather_label, colors->weather, LV_PART_MAIN);
    lv_obj_set_style_text_color(date_label, colors->clock_on, LV_PART_MAIN);
    lv_obj_set_style_text_color(seconds_label, colors->data_val, LV_PART_MAIN);

    lv_obj_set_style_text_color(field1_label, colors->field_lbl, LV_PART_MAIN);
    lv_obj_set_style_text_color(field1_value, colors->heart_rate, LV_PART_MAIN);
    lv_obj_set_style_text_color(field2_label, colors->field_lbl, LV_PART_MAIN);
    lv_obj_set_style_text_color(field2_value, colors->steps, LV_PART_MAIN);
    lv_obj_set_style_text_color(field3_label, colors->field_lbl, LV_PART_MAIN);
    lv_obj_set_style_text_color(field3_value, colors->accent, LV_PART_MAIN);

    lv_obj_set_style_text_color(bottom5_label, colors->clock_on, LV_PART_MAIN);
    lv_obj_set_style_bg_color(stress_bar, colors->stress, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bodybatt_bar, colors->bodybatt, LV_PART_MAIN);
}
