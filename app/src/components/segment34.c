#include "segment34.h"

static const uint16_t segment_masks[10] = {
    0x3FF, 0x060, 0x5DB, 0x5FB, 0x66B, 0x7BB, 0x7FB, 0x0EB, 0x7FF, 0x6FB
};

static const int seg_positions[17][2] = {
    {0, 0}, {1, 0}, {2, 0},
    {0, 1},          {2, 1},
    {0, 2}, {1, 2}, {2, 2},
    {0, 3},          {2, 3},
    {0, 4}, {1, 4}, {2, 4},
    {0, 5},          {2, 5},
    {0, 6}, {1, 6}, {2, 6}
};

void segment34_init(segment34_t *seg, lv_obj_t *parent, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    seg->parent = parent;
    seg->width = width;
    seg->height = height;
    seg->color_on = lv_color_hex(0x00FF00);
    seg->color_off = lv_color_hex(0x1a1a1a);
    seg->show_colon = true;
    
    uint16_t digit_w = width / 5;
    uint16_t digit_h = height;
    uint16_t seg_w = digit_w / 4;
    uint16_t seg_h = digit_h / 8;
    uint16_t colon_spacing = 10;
    
    for (int digit = 0; digit < SEGMENT34_DIGITS; digit++) {
        uint16_t dx = x;
        if (digit < 2) {
            dx += digit * (digit_w + colon_spacing / 2);
        } else {
            dx += digit * (digit_w + colon_spacing / 2) + colon_spacing;
        }
        
        for (int i = 0; i < SEGMENT34_SEGMENTS_PER_DIGIT; i++) {
            lv_obj_t *segment = lv_obj_create(parent);
            lv_obj_set_size(segment, seg_w - 2, seg_h - 2);
            lv_obj_set_pos(segment, dx + seg_positions[i][0] * seg_w, y + seg_positions[i][1] * seg_h);
            lv_obj_set_style_bg_color(segment, seg->color_off, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(segment, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_width(segment, 0, LV_PART_MAIN);
            seg->segments[digit * SEGMENT34_SEGMENTS_PER_DIGIT + i] = segment;
        }
    }
    
    uint16_t colon_x = x + 2 * digit_w + colon_spacing / 2;
    
    seg->colon1 = lv_obj_create(parent);
    lv_obj_set_size(seg->colon1, 6, 6);
    lv_obj_set_pos(seg->colon1, colon_x, y + height / 2 - 12);
    lv_obj_set_style_bg_color(seg->colon1, seg->color_on, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(seg->colon1, LV_OPA_COVER, LV_PART_MAIN);
    
    seg->colon2 = lv_obj_create(parent);
    lv_obj_set_size(seg->colon2, 6, 6);
    lv_obj_set_pos(seg->colon2, colon_x, y + height / 2 + 6);
    lv_obj_set_style_bg_color(seg->colon2, seg->color_on, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(seg->colon2, LV_OPA_COVER, LV_PART_MAIN);
}

void segment34_set_color(segment34_t *seg, lv_color_t on, lv_color_t off)
{
    seg->color_on = on;
    seg->color_off = off;
    
    for (int i = 0; i < SEGMENT34_TOTAL_SEGMENTS; i++) {
        if (seg->segments[i]) {
            lv_obj_set_style_bg_color(seg->segments[i], off, LV_PART_MAIN);
        }
    }
    
    lv_color_t colon_color = seg->show_colon ? on : off;
    if (seg->colon1) {
        lv_obj_set_style_bg_color(seg->colon1, colon_color, LV_PART_MAIN);
    }
    if (seg->colon2) {
        lv_obj_set_style_bg_color(seg->colon2, colon_color, LV_PART_MAIN);
    }
}

static void segment34_update_digit(segment34_t *seg, uint8_t digit_idx, uint8_t value)
{
    uint16_t mask = segment_masks[value];
    uint8_t start = digit_idx * SEGMENT34_SEGMENTS_PER_DIGIT;
    
    for (int i = 0; i < SEGMENT34_SEGMENTS_PER_DIGIT; i++) {
        if (seg->segments[start + i]) {
            if (mask & (1 << i)) {
                lv_obj_set_style_bg_color(seg->segments[start + i], seg->color_on, LV_PART_MAIN);
            } else {
                lv_obj_set_style_bg_color(seg->segments[start + i], seg->color_off, LV_PART_MAIN);
            }
        }
    }
}

void segment34_set_time(segment34_t *seg, uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    segment34_update_digit(seg, 0, hours / 10);
    segment34_update_digit(seg, 1, hours % 10);
    segment34_update_digit(seg, 2, minutes / 10);
    segment34_update_digit(seg, 3, minutes % 10);
    
    (void)seconds;
}

void segment34_update_colon(segment34_t *seg, bool show)
{
    seg->show_colon = show;
    lv_color_t color = show ? seg->color_on : seg->color_off;
    
    if (seg->colon1) {
        lv_obj_set_style_bg_color(seg->colon1, color, LV_PART_MAIN);
    }
    if (seg->colon2) {
        lv_obj_set_style_bg_color(seg->colon2, color, LV_PART_MAIN);
    }
}

void segment34_delete(segment34_t *seg)
{
    for (int i = 0; i < SEGMENT34_TOTAL_SEGMENTS; i++) {
        if (seg->segments[i]) {
            lv_obj_del(seg->segments[i]);
        }
    }
    if (seg->colon1) {
        lv_obj_del(seg->colon1);
    }
    if (seg->colon2) {
        lv_obj_del(seg->colon2);
    }
}