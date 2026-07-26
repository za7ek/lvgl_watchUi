#ifndef SEGMENT34_H
#define SEGMENT34_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEGMENT34_DIGITS 4
#define SEGMENT34_SEGMENTS_PER_DIGIT 17
#define SEGMENT34_TOTAL_SEGMENTS (SEGMENT34_DIGITS * SEGMENT34_SEGMENTS_PER_DIGIT)

typedef struct {
    lv_obj_t *obj;
    uint16_t width;
    uint16_t height;
    lv_color_t color_on;
    lv_color_t color_off;
    bool show_colon;
    uint8_t digits[SEGMENT34_DIGITS];
} segment34_t;

void segment34_init(segment34_t *seg, lv_obj_t *parent, uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void segment34_set_color(segment34_t *seg, lv_color_t on, lv_color_t off);
void segment34_set_time(segment34_t *seg, uint8_t hours, uint8_t minutes, uint8_t seconds);
void segment34_update_colon(segment34_t *seg, bool show);
void segment34_delete(segment34_t *seg);

#ifdef __cplusplus
}
#endif

#endif
