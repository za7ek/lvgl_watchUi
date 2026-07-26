#ifndef THEME_H
#define THEME_H

#include <lvgl.h>

typedef enum {
    THEME_GREEN,
    THEME_BLUE,
    THEME_RED,
    THEME_ORANGE,
    THEME_PURPLE,
    THEME_CYAN,
    THEME_COUNT
} theme_color_t;

typedef struct {
    const char *name;
    lv_color_t bg;
    lv_color_t clock_on;
    lv_color_t clock_off;
    lv_color_t text;
    lv_color_t accent;
    lv_color_t weather;
    lv_color_t heart_rate;
    lv_color_t steps;
    lv_color_t battery;
    lv_color_t field_lbl;
    lv_color_t field_bg;
    lv_color_t data_val;
    lv_color_t stress;
    lv_color_t bodybatt;
    lv_color_t notif;
    lv_color_t moon;
    lv_color_t outline;
} theme_colors_t;

void theme_init(theme_color_t theme);
const theme_colors_t *theme_get_colors(void);
theme_color_t theme_get_current(void);
void theme_set_current(theme_color_t theme);
const char *theme_get_name(theme_color_t theme);

#endif