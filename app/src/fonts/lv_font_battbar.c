/*
 * lv_font_battbar —— 电池填充条专用字体（手写，不由 gen_font.py 生成）
 *
 * 为什么不复用 lv_font_xsmol：
 *   1. Segment34.CN 的 xsmol.fnt 里根本没有 '|'(0x7C) 和 '{'(0x7B) 这两个字符，
 *      只有 13px 的 smol.fnt 才有。以前 lv_font_xsmol 是拿 smol.fnt 生成的，
 *      标签字体因此整整大了 3px、RECOVERY HRS: 撑到 74px 溢出容器 —— 那是 bug，
 *      不是特性。xsmol 换回真正的 10px 源之后，这两个字符自然就没了。
 *   2. 就算 smol 里有，它的 '|' 是 1×8 的框、只有 7 行实心，塞进 12px 高、
 *      内框 10px 的电池里怎么摆都不对称（这正是"白色紧贴上边框、下边框留缝"
 *      的来源）。填充条要的是"正好等于电池内框高度"，图集里没有这种尺寸。
 *
 * 所以这里直接按电池几何写死：line_height = 8，字形 1×8 满格实心，ofs_y = 0。
 * LVGL 的字形落点是 y1 = pos.y + (line_height - base_line) - box_h - ofs_y
 * = pos.y + 0，即标签 y 坐标就是条的顶边，摆放时不用再脑补偏移。
 * 电池外框 24×12、border 1 → 内容区 22×10；条放在内容坐标 (1,1)、20×8，
 * 上下左右各留 1px，绝对占 y2..y9 —— 垂直居中。
 *
 * 字符含义同 Segment34.CN 的 battFull / battEmpty：
 *   '|' = 实心格（有电），'{' = 空心格（没电，全透明占位）
 * adv_w = 16（1.0px）且 box_w = 1，20 个字符首尾相接正好 20px，无缝填充。
 */

#include "lvgl.h"

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    0x00,   /* '{' 空心：8 行全透明 */
    0xFF    /* '|' 实心：8 行全前景 */
};

/*Describe the properties of all glyphs*/
static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {     0,    0,   0,   0,  0,  0 },  /* reserved dummy */
    {     0,   16,   1,   8,  0,  0 },  /* 0x007B '{' */
    {     1,   16,   1,   8,  0,  0 }   /* 0x007C '|' */
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] = {
    {
        .range_start = 123, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 124, .range_length = 1, .glyph_id_start = 2,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*Store all the custom data of the font*/
static const lv_font_fmt_txt_dsc_t font_dsc = {
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 2,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
};

/*Initialize a public general font descriptor*/
const lv_font_t lv_font_battbar = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 8,
    .base_line = 0,
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = 0,
    .underline_thickness = 0,
    .dsc = &font_dsc
};
