#ifndef LV_FONT_ICONS_H
#define LV_FONT_ICONS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 状态图标字体，21px 高，字形取自 Segment34 的 icons.fnt/icons.png：
 *   'A' 闹钟   'D' 勿扰   'L' 蓝牙   'N'-'R' 久坐提醒条 1-5 级
 *
 * 参考图集里还有个 'M'（蓝牙断开），但它和 'L' 逐像素相同、只是整个画成 85/255
 * 的灰度，1bpp 装不下这个区别，所以没收进来 —— 断开状态用 'L' 配暗色表示。
 * 字形与状态的对应关系见 watchface.c 的 icon_slot_glyph()。 */
extern const lv_font_t lv_font_icons;

#ifdef __cplusplus
}
#endif

#endif
