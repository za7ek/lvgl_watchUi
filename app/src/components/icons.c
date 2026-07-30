#include "icons.h"
#include <string.h>

#define ICON_SIZE 16
#define PIXEL_SIZE 1

typedef struct {
    icon_t icon;
    lv_color_t color;
} icon_info_t;

static const uint8_t icon_heart[8] = {
    0b01100110,
    0b11111111,
    0b11111111,
    0b11111111,
    0b01111110,
    0b00111100,
    0b00011000,
    0b00000000,
};

static const uint8_t icon_steps[11] = {
    0b00011000,
    0b00111100,
    0b01111110,
    0b01111110,
    0b00111100,
    0b00011000,
    0b00111100,
    0b01111110,
    0b01111110,
    0b00111100,
    0b00011000,
};

static const uint8_t icon_battery_full[8] = {
    0b01111110,
    0b11111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b01111110,
};

static const uint8_t icon_battery_empty[8] = {
    0b01111110,
    0b10000001,
    0b10000001,
    0b10000001,
    0b10000001,
    0b10000001,
    0b10000001,
    0b01111110,
};

static const uint8_t icon_alarm[10] = {
    0b00011000,
    0b00111100,
    0b01111110,
    0b01111110,
    0b01111110,
    0b01111110,
    0b00111100,
    0b00011000,
    0b00111100,
    0b00000000,
};

static const uint8_t icon_bluetooth[9] = {
    0b00010000,
    0b00111000,
    0b01101100,
    0b01000100,
    0b00101000,
    0b01000100,
    0b01101100,
    0b00111000,
    0b00010000,
};

static const uint8_t icon_moon[9] = {
    0b00111100,
    0b01111110,
    0b11100111,
    0b11000011,
    0b11000011,
    0b11000011,
    0b11100111,
    0b01111110,
    0b00111100,
};

static const uint8_t icon_arrow_up[9] = {
    0b00011000,
    0b00111100,
    0b01111110,
    0b11111111,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00000000,
};

static const uint8_t icon_arrow_down[9] = {
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b11111111,
    0b01111110,
    0b00111100,
    0b00011000,
    0b00000000,
};

static const uint8_t icon_calories[9] = {
    0b00111100,
    0b01000010,
    0b10011001,
    0b10100101,
    0b10100101,
    0b10011001,
    0b01000010,
    0b00111100,
    0b00000000,
};

static const uint8_t *get_icon_data(icon_t icon, int *height)
{
    switch (icon) {
    case ICON_HEART:       *height = 8;  return icon_heart;
    case ICON_STEPS:       *height = 11; return icon_steps;
    case ICON_BATTERY_FULL:*height = 8;  return icon_battery_full;
    case ICON_BATTERY_EMPTY:*height = 8; return icon_battery_empty;
    case ICON_ALARM:       *height = 10; return icon_alarm;
    case ICON_BLUETOOTH:   *height = 9;  return icon_bluetooth;
    case ICON_MOON:        *height = 9;  return icon_moon;
    case ICON_ARROW_UP:    *height = 9;  return icon_arrow_up;
    case ICON_ARROW_DOWN:  *height = 9;  return icon_arrow_down;
    case ICON_CALORIES:    *height = 9;  return icon_calories;
    default:               *height = 0;  return NULL;
    }
}

static void icon_draw_event_cb(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_obj_t *obj = lv_event_get_target(e);
    icon_info_t *info = (icon_info_t *)lv_obj_get_user_data(obj);
    if (!info) return;

    int h;
    const uint8_t *data = get_icon_data(info->icon, &h);
    if (!data || h == 0) return;

    lv_color_t color = info->color;

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = 0;

    lv_area_t area;
    int w = 8;

    for (int y = 0; y < h; y++) {
        uint8_t row = data[y];
        for (int x = 0; x < w; x++) {
            if (row & (1 << (7 - x))) {
                area.x1 = x;
                area.y1 = y;
                area.x2 = x;
                area.y2 = y;
                lv_draw_rect(layer, &dsc, &area);
            }
        }
    }
}

void icon_draw(lv_obj_t *parent, icon_t icon, lv_coord_t x, lv_coord_t y, lv_color_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, 8, 16);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);

    icon_info_t *info = (icon_info_t *)lv_malloc(sizeof(icon_info_t));
    if (!info) {
        lv_obj_del(obj);
        return;
    }
    info->icon = icon;
    info->color = color;
    lv_obj_set_user_data(obj, info);

    lv_obj_add_event_cb(obj, icon_draw_event_cb, LV_EVENT_DRAW_MAIN, NULL);
}
