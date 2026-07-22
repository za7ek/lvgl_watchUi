#include "watchface.h"
#include "segment34.h"
#include "lunar_calendar.h"
#include "locale.h"
#include "theme.h"
#include "lv_font_cjk.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/timeutil.h>
#include <time.h>

LOG_MODULE_REGISTER(watchface, LOG_LEVEL_INF);

static lv_obj_t *root_page = NULL;
static lv_obj_t *date_label = NULL;
static lv_obj_t *lunar_label = NULL;
static lv_obj_t *weather_label = NULL;
static lv_obj_t *heart_rate_label = NULL;
static lv_obj_t *steps_label = NULL;
static lv_obj_t *battery_label = NULL;
static lv_obj_t *battery_bar = NULL;

static segment34_t segment_clock;

static lv_timer_t *time_update_timer = NULL;
static lv_timer_t *sensor_update_timer = NULL;

static void time_update_callback(lv_timer_t *timer)
{
    watchface_update_time();
    watchface_update_date();
    ARG_UNUSED(timer);
}

static void sensor_update_callback(lv_timer_t *timer)
{
    watchface_update_sensors();
    ARG_UNUSED(timer);
}

void watchface_start(void)
{
    theme_init(THEME_GREEN);
    const theme_colors_t *colors = theme_get_colors();
    
    root_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(root_page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(root_page, colors->bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_page, LV_OPA_COVER, LV_PART_MAIN);
    
    lv_obj_set_style_border_width(root_page, 0, LV_PART_MAIN);
    
    lv_obj_t *main_container = lv_obj_create(root_page);
    lv_obj_set_size(main_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(main_container, colors->bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main_container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
    lv_obj_center(main_container);
    
    segment34_init(&segment_clock, main_container, 10, 50, 220, 80);
    segment34_set_color(&segment_clock, colors->clock_on, colors->clock_off);
    
    date_label = lv_label_create(main_container);
    lv_obj_set_pos(date_label, 20, 150);
    lv_obj_set_style_text_color(date_label, colors->text, LV_PART_MAIN);
    lv_obj_set_style_text_font(date_label, &lv_font_cjk_16, LV_PART_MAIN);
    
    lunar_label = lv_label_create(main_container);
    lv_obj_set_pos(lunar_label, 20, 175);
    lv_obj_set_style_text_color(lunar_label, colors->accent, LV_PART_MAIN);
    lv_obj_set_style_text_font(lunar_label, &lv_font_cjk_16, LV_PART_MAIN);
    
    weather_label = lv_label_create(main_container);
    lv_obj_set_pos(weather_label, 130, 150);
    lv_obj_set_style_text_color(weather_label, colors->weather, LV_PART_MAIN);
    lv_obj_set_style_text_font(weather_label, &lv_font_cjk_16, LV_PART_MAIN);
    
    heart_rate_label = lv_label_create(main_container);
    lv_obj_set_pos(heart_rate_label, 20, 200);
    lv_obj_set_style_text_color(heart_rate_label, colors->heart_rate, LV_PART_MAIN);
    lv_obj_set_style_text_font(heart_rate_label, &lv_font_cjk_16, LV_PART_MAIN);
    
    steps_label = lv_label_create(main_container);
    lv_obj_set_pos(steps_label, 90, 200);
    lv_obj_set_style_text_color(steps_label, colors->steps, LV_PART_MAIN);
    lv_obj_set_style_text_font(steps_label, &lv_font_cjk_16, LV_PART_MAIN);
    
    battery_label = lv_label_create(main_container);
    lv_obj_set_pos(battery_label, 160, 200);
    lv_obj_set_style_text_color(battery_label, colors->battery, LV_PART_MAIN);
    lv_obj_set_style_text_font(battery_label, &lv_font_cjk_16, LV_PART_MAIN);
    
    battery_bar = lv_bar_create(main_container);
    lv_obj_set_size(battery_bar, 40, 6);
    lv_obj_set_pos(battery_bar, 160, 215);
    lv_obj_set_style_bg_color(battery_bar, colors->clock_off, LV_PART_MAIN);
    lv_obj_set_style_bg_color(battery_bar, colors->battery, LV_PART_INDICATOR);
    lv_bar_set_value(battery_bar, 85, LV_ANIM_OFF);
    
    watchface_update_time();
    watchface_update_date();
    watchface_update_weather();
    watchface_update_sensors();
    
    time_update_timer = lv_timer_create(time_update_callback, 1000, NULL);
    sensor_update_timer = lv_timer_create(sensor_update_callback, 10000, NULL);
}

void watchface_stop(void)
{
    if (time_update_timer) {
        lv_timer_del(time_update_timer);
        time_update_timer = NULL;
    }
    if (sensor_update_timer) {
        lv_timer_del(sensor_update_timer);
        sensor_update_timer = NULL;
    }
    
    segment34_delete(&segment_clock);
    
    if (root_page) {
        lv_obj_del(root_page);
        root_page = NULL;
    }
}

void watchface_update_time(void)
{
    struct tm timeinfo;
    time_t now = time(NULL);
    localtime_r(&now, &timeinfo);
    
    uint8_t hours = timeinfo.tm_hour;
    uint8_t minutes = timeinfo.tm_min;
    uint8_t seconds = timeinfo.tm_sec;
    
    segment34_set_time(&segment_clock, hours, minutes, seconds);
    segment34_update_colon(&segment_clock, seconds % 2 == 0);
}

void watchface_update_date(void)
{
    struct tm timeinfo;
    time_t now = time(NULL);
    localtime_r(&now, &timeinfo);
    
    char date_str[64];
    const char *weekday_str = locale_get_string(LOCALE_STR_MONDAY + timeinfo.tm_wday);
    
    if (current_lang == LANG_EN) {
        snprintf(date_str, sizeof(date_str), "%s, %d %s %d",
                 weekday_str, timeinfo.tm_mday,
                 locale_get_string(LOCALE_STR_JANUARY + timeinfo.tm_mon),
                 timeinfo.tm_year + 1900);
    } else {
        snprintf(date_str, sizeof(date_str), "%d年%d月%d日 %s",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 weekday_str);
    }
    
    lv_label_set_text(date_label, date_str);
    
#ifdef CONFIG_SEGMENT34_SHOW_LUNAR
    lunar_date_t lunar;
    lunar_calendar_convert(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, &lunar);
    
    char lunar_str[32];
    snprintf(lunar_str, sizeof(lunar_str), "%s%s", lunar.month_name, lunar.day_name);
    lv_label_set_text(lunar_label, lunar_str);
#endif
}

void watchface_update_weather(void)
{
    if (!weather_label) return;
    
    char weather_str[32];
    snprintf(weather_str, sizeof(weather_str), "%s: 26C", locale_get_string(LOCALE_STR_CLEAR));
    lv_label_set_text(weather_label, weather_str);
}

void watchface_update_sensors(void)
{
    if (heart_rate_label) {
        char hr_str[16];
        snprintf(hr_str, sizeof(hr_str), "%s: 72", locale_get_string(LOCALE_STR_HEART_RATE));
        lv_label_set_text(heart_rate_label, hr_str);
    }
    
    if (steps_label) {
        char steps_str[16];
        snprintf(steps_str, sizeof(steps_str), "%s: 8542", locale_get_string(LOCALE_STR_STEPS));
        lv_label_set_text(steps_label, steps_str);
    }
    
    if (battery_label) {
        char battery_str[16];
        snprintf(battery_str, sizeof(battery_str), "%s: 85%%", locale_get_string(LOCALE_STR_BATTERY));
        lv_label_set_text(battery_label, battery_str);
    }
}

void watchface_switch_language(void)
{
    if (current_lang == LANG_ZH) {
        locale_set_current(LANG_EN);
    } else {
        locale_set_current(LANG_ZH);
    }
    watchface_update_date();
}

void watchface_switch_theme(void)
{
    theme_color_t current = theme_get_current();
    theme_color_t next = (current + 1) % THEME_COUNT;
    theme_set_current(next);
    
    const theme_colors_t *colors = theme_get_colors();
    
    lv_obj_set_style_bg_color(root_page, colors->bg, LV_PART_MAIN);
    lv_obj_set_style_bg_color(lv_obj_get_parent(date_label), colors->bg, LV_PART_MAIN);
    
    segment34_set_color(&segment_clock, colors->clock_on, colors->clock_off);
    
    lv_obj_set_style_text_color(date_label, colors->text, LV_PART_MAIN);
    lv_obj_set_style_text_color(lunar_label, colors->accent, LV_PART_MAIN);
    lv_obj_set_style_text_color(weather_label, colors->weather, LV_PART_MAIN);
    lv_obj_set_style_text_color(heart_rate_label, colors->heart_rate, LV_PART_MAIN);
    lv_obj_set_style_text_color(steps_label, colors->steps, LV_PART_MAIN);
    lv_obj_set_style_text_color(battery_label, colors->battery, LV_PART_MAIN);
    
    lv_obj_set_style_bg_color(battery_bar, colors->clock_off, LV_PART_MAIN);
    lv_obj_set_style_bg_color(battery_bar, colors->battery, LV_PART_INDICATOR);
}