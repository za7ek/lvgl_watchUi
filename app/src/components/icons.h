#ifndef ICONS_H
#define ICONS_H

#include <lvgl.h>

typedef enum {
    ICON_HEART,
    ICON_STEPS,
    ICON_BATTERY_FULL,
    ICON_BATTERY_EMPTY,
    ICON_ALARM,
    ICON_BLUETOOTH,
    ICON_MOON,
    ICON_ARROW_UP,
    ICON_ARROW_DOWN,
    ICON_CALORIES,
    ICON_COUNT
} icon_t;

void icon_draw(lv_obj_t *parent, icon_t icon, lv_coord_t x, lv_coord_t y, lv_color_t color);

#endif
