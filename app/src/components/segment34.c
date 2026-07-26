#include "segment34.h"
#include <stdlib.h>
#include <string.h>

static const uint16_t segment_masks_7seg[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

static void segment34_draw_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    segment34_t *seg = (segment34_t *)lv_obj_get_user_data(obj);
    if (!seg) return;

    lv_layer_t *layer = lv_event_get_layer(e);

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.radius = 1;
    rect_dsc.bg_opa = LV_OPA_COVER;

    uint16_t digit_w = seg->width / 5;
    uint16_t digit_h = seg->height;
    uint16_t sw = digit_w / 4;
    uint16_t sh = digit_h / 7;
    uint16_t colon_spacing = 8;

    for (int digit = 0; digit < SEGMENT34_DIGITS; digit++) {
        uint16_t digit_x;
        if (digit < 2) {
            digit_x = digit * (digit_w + colon_spacing / 2);
        } else {
            digit_x = digit * (digit_w + colon_spacing / 2) + colon_spacing;
        }

        uint16_t mask = segment_masks_7seg[seg->digits[digit]];
        int16_t dx = digit_x + (digit_w - sw * 2 - 2) / 2;
        int16_t dy = (digit_h - sh * 7) / 2;

        lv_draw_rect_dsc_t dsc = rect_dsc;
        dsc.bg_color = seg->color_on;

        lv_area_t sa = {dx, dy, dx + sw * 2 + 2, dy + sh};
        lv_area_t sb = {dx + sw + 2, dy + sh, dx + sw * 2 + 2, dy + sh * 4};
        lv_area_t sc = {dx + sw + 2, dy + sh * 4, dx + sw * 2 + 2, dy + sh * 7};
        lv_area_t sd = {dx, dy + sh * 6, dx + sw * 2 + 2, dy + sh * 7};
        lv_area_t se = {dx, dy + sh * 4, dx + sw, dy + sh * 7};
        lv_area_t sf = {dx, dy + sh, dx + sw, dy + sh * 4};
        lv_area_t sg = {dx, dy + sh * 3, dx + sw * 2 + 2, dy + sh * 4};

        if (mask & 0x01) lv_draw_rect(layer, &dsc, &sa);
        if (mask & 0x02) lv_draw_rect(layer, &dsc, &sb);
        if (mask & 0x04) lv_draw_rect(layer, &dsc, &sc);
        if (mask & 0x08) lv_draw_rect(layer, &dsc, &sd);
        if (mask & 0x10) lv_draw_rect(layer, &dsc, &se);
        if (mask & 0x20) lv_draw_rect(layer, &dsc, &sf);
        if (mask & 0x40) lv_draw_rect(layer, &dsc, &sg);
    }

    if (seg->show_colon) {
        uint16_t colon_x = 2 * digit_w + colon_spacing / 2;
        uint16_t colon_y1 = seg->height / 2 - 8;
        uint16_t colon_y2 = seg->height / 2 + 4;

        lv_draw_rect_dsc_t colon_dsc = rect_dsc;
        colon_dsc.bg_color = seg->color_on;

        lv_area_t dot1 = {colon_x, colon_y1, colon_x + 4, colon_y1 + 4};
        lv_area_t dot2 = {colon_x, colon_y2, colon_x + 4, colon_y2 + 4};

        lv_draw_rect(layer, &colon_dsc, &dot1);
        lv_draw_rect(layer, &colon_dsc, &dot2);
    }
}

void segment34_init(segment34_t *seg, lv_obj_t *parent, uint16_t x, uint16_t y,
                    uint16_t width, uint16_t height)
{
    seg->width = width;
    seg->height = height;
    seg->color_on = lv_color_hex(0xFFAA00);
    seg->color_off = lv_color_hex(0x2A1A0A);
    seg->show_colon = true;
    seg->digits[0] = 0;
    seg->digits[1] = 0;
    seg->digits[2] = 0;
    seg->digits[3] = 0;

    seg->obj = lv_obj_create(parent);
    lv_obj_set_size(seg->obj, width, height);
    lv_obj_set_pos(seg->obj, x, y);
    lv_obj_set_style_bg_opa(seg->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(seg->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(seg->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(seg->obj, 0, LV_PART_MAIN);
    lv_obj_set_user_data(seg->obj, seg);
    lv_obj_add_event_cb(seg->obj, segment34_draw_event_cb, LV_EVENT_DRAW_MAIN, NULL);
}

void segment34_set_color(segment34_t *seg, lv_color_t on, lv_color_t off)
{
    seg->color_on = on;
    seg->color_off = off;
    if (seg->obj) {
        lv_obj_invalidate(seg->obj);
    }
}

void segment34_set_time(segment34_t *seg, uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    seg->digits[0] = hours / 10;
    seg->digits[1] = hours % 10;
    seg->digits[2] = minutes / 10;
    seg->digits[3] = minutes % 10;
    if (seg->obj) {
        lv_obj_invalidate(seg->obj);
    }
    (void)seconds;
}

void segment34_update_colon(segment34_t *seg, bool show)
{
    seg->show_colon = show;
    if (seg->obj) {
        lv_obj_invalidate(seg->obj);
    }
}

void segment34_delete(segment34_t *seg)
{
    if (seg->obj) {
        lv_obj_del(seg->obj);
        seg->obj = NULL;
    }
}
