#ifndef ICONS_H
#define ICONS_H

#include <lvgl.h>

/* 只保留表盘实际画出来的两个图标；其余（steps/battery/alarm/bluetooth/
 * moon/arrow）从未被 icon_draw() 引用，已随位图一起删除。 */
typedef enum {
    ICON_HEART,
    ICON_CALORIES,
    ICON_COUNT
} icon_t;

void icon_draw(lv_obj_t *parent, icon_t icon, lv_coord_t x, lv_coord_t y, lv_color_t color);

#endif
