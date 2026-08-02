#!/usr/bin/env python3
import os
from PIL import Image, ImageDraw, ImageFont

TTF_PATH = "/usr/share/fonts/win11/simsun.ttc"
FONT_SIZE = 10
BPP = 4
OUT_PATH = "/home/zheng_fang/zephyr-project/lgvl_watchUi/app/src/fonts/lv_font_cjk.c"
ASCII_START = 0x20
ASCII_END = 0x7E
ASCII_COUNT = ASCII_END - ASCII_START + 1
CJK_RANGE_START = 0x4E00
CJK_STRINGS = [
    u"一二三四五六七八九十",
    u"月日年星期天气",
    u"晴多云阴雨雪",
    u"心率步数电池",
    u"中文",
    u"甲乙丙丁戊己庚辛壬癸",
    u"子丑寅卯辰巳午未申酉戌亥",
    u"鼠牛虎兔龙蛇马羊猴鸡狗猪",
    u"正冬腊",
    u"初廿",
    u"闰",
    u"出落新上弦满下卡路里",
    u"恢复小时周活动分楼层",
]
FONT_NAME = "lv_font_cjk"
LINE_HEIGHT = 14
BASE_LINE = 2

def build_char_lists():
    ascii_chars = [chr(c) for c in range(ASCII_START, ASCII_END + 1)]
    cjk = set()
    for s in CJK_STRINGS:
        for ch in s:
            cjk.add(ch)
    return ascii_chars, sorted(cjk, key=ord)

def render_glyph(font, ch):
    ascent, descent = font.getmetrics()
    advance = font.getlength(ch)
    adv_w = int(round(advance * 16))
    if adv_w < 1:
        adv_w = 1
    margin = 8
    baseline_y = margin + ascent
    canvas_w = int(advance) + 2 * margin + 8
    canvas_h = ascent + descent + 2 * margin
    img = Image.new("L", (canvas_w, canvas_h), 0)
    draw = ImageDraw.Draw(img)
    draw.text((margin, baseline_y), ch, font=font, fill=255, anchor="ls")
    bbox = img.getbbox()
    if bbox is None:
        return {"box_w": 0, "box_h": 0, "ofs_x": 0, "ofs_y": 0, "adv_w": adv_w, "data": b""}
    x0, y0, x1, y1 = bbox
    box_w = x1 - x0
    box_h = y1 - y0
    ofs_x = x0 - margin
    ofs_y = baseline_y - y1
    pixels = list(img.crop(bbox).getdata())
    nibbles = [(p * 15 + 127) // 255 for p in pixels]
    row_bytes = (box_w + 1) // 2
    data = bytearray(row_bytes * box_h)
    for row in range(box_h):
        for col in range(box_w):
            nib = nibbles[row * box_w + col] & 0x0F
            byte_idx = row * row_bytes + col // 2
            if col % 2 == 0:
                data[byte_idx] |= nib << 4
            else:
                data[byte_idx] |= nib
    return {"box_w": box_w, "box_h": box_h, "ofs_x": ofs_x, "ofs_y": ofs_y, "adv_w": adv_w, "data": bytes(data)}

def main():
    ascii_chars, cjk_chars = build_char_lists()
    font = ImageFont.truetype(TTF_PATH, FONT_SIZE)
    ascent, descent = font.getmetrics()
    line_height = ascent + descent
    base_line = descent

    glyphs = [{"box_w": 0, "box_h": 0, "ofs_x": 0, "ofs_y": 0, "adv_w": 0, "data": b""}]
    for ch in ascii_chars:
        glyphs.append(render_glyph(font, ch))
    cjk_start = len(glyphs)
    for ch in cjk_chars:
        glyphs.append(render_glyph(font, ch))

    bitmap = bytearray()
    glyph_dsc_lines = []
    for g in glyphs:
        idx = len(bitmap)
        glyph_dsc_lines.append(
            "    {.bitmap_index = %d, .adv_w = %d, .box_w = %d, .box_h = %d, .ofs_x = %d, .ofs_y = %d},"
            % (idx, g["adv_w"], g["box_w"], g["box_h"], g["ofs_x"], g["ofs_y"]))
        bitmap.extend(g["data"])

    cjk_codepoints = [ord(ch) for ch in cjk_chars]
    cjk_min = min(cjk_codepoints)
    cjk_max = max(cjk_codepoints)
    cmap_num = 1 + len(cjk_chars)  # 1 ASCII block + 1 per CJK char
    L = []
    L.append('#include "lv_font_cjk.h"')
    L.append('')
    L.append('/* Auto-generated CJK font: %d glyphs (%d ASCII + %d CJK + 1 placeholder), %d bytes bitmap, %dbpp */'
             % (len(glyphs), len(ascii_chars), len(cjk_chars), len(bitmap), BPP))
    L.append('')
    L.append('static const uint8_t %s_bitmap[%d] = {' % (FONT_NAME, len(bitmap)))
    for i in range(0, len(bitmap), 16):
        chunk = bitmap[i:i+16]
        L.append('    ' + ', '.join('0x%02x' % b for b in chunk) + ',')
    if not bitmap:
        L.append('    0')
    L.append('};')
    L.append('')
    L.append('static const lv_font_fmt_txt_glyph_dsc_t %s_glyph_dsc[] = {' % FONT_NAME)
    L.extend(glyph_dsc_lines)
    L.append('};')
    L.append('')
    L.append('')
    # CJK cmap: 1 FORMAT0_TINY block for ASCII + 1 FORMAT0_TINY per CJK char.
    # This matches gen_font.py's approach (proven to work with this LVGL version).
    # SPARSE_TINY was used before but caused all CJK chars to render as boxes.
    L.append('static const lv_font_fmt_txt_cmap_t %s_cmap[] = {' % FONT_NAME)
    L.append('    {')
    L.append('        .range_start = 0x%02x, .range_length = %d, .glyph_id_start = 1,' % (ASCII_START, ASCII_COUNT))
    L.append('        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = %d,' % ASCII_COUNT)
    L.append('        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY,')
    L.append('    },')
    for i, cp in enumerate(cjk_codepoints):
        comma = ',' if i < len(cjk_codepoints) - 1 else ''
        L.append('    {')
        L.append('        .range_start = 0x%04x, .range_length = 1, .glyph_id_start = %d,' % (cp, cjk_start + i))
        L.append('        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0,')
        L.append('        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY,')
        L.append('    }%s' % comma)
    L.append('};')
    L.append('')
    L.append('static const lv_font_fmt_txt_dsc_t %s_fmt_dsc = {' % FONT_NAME)
    L.append('    .glyph_bitmap = %s_bitmap,' % FONT_NAME)
    L.append('    .glyph_dsc = %s_glyph_dsc,' % FONT_NAME)
    L.append('    .cmaps = %s_cmap,' % FONT_NAME)
    L.append('    .kern_dsc = NULL,')
    L.append('    .kern_scale = 0,')
    L.append('    .cmap_num = %d,' % cmap_num)
    L.append('    .bpp = %d,' % BPP)
    L.append('    .kern_classes = 0,')
    L.append('    .bitmap_format = 0,')
    L.append('};')
    L.append('')
    L.append('const lv_font_t %s = {' % FONT_NAME)
    L.append('    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,')
    L.append('    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,')
    L.append('    .line_height = %d,' % line_height)
    L.append('    .base_line = %d,' % base_line)
    L.append('    .subpx = LV_FONT_SUBPX_NONE,')
    L.append('    .underline_position = -1,')
    L.append('    .underline_thickness = 1,')
    L.append('    .dsc = &%s_fmt_dsc,' % FONT_NAME)
    L.append('};')
    L.append('')

    with open(OUT_PATH, 'w', encoding='utf-8') as f:
        f.write('\n'.join(L))

    print('Generated %s' % OUT_PATH)
    print('  Total glyphs: %d (1 placeholder + %d ASCII + %d CJK)' % (len(glyphs), len(ascii_chars), len(cjk_chars)))
    print('  Bitmap size: %d bytes' % len(bitmap))
    print('  ASCII: 0x%02X-0x%02X -> glyph IDs 1-%d' % (ASCII_START, ASCII_END, ASCII_COUNT))
    print('  CJK: %d chars, range 0x%04X-0x%04X -> glyph IDs %d-%d' % (len(cjk_chars), cjk_min, cjk_max, cjk_start, cjk_start + len(cjk_chars) - 1))
    print('  cmap_num: %d (1 ASCII + %d CJK individual)' % (cmap_num, len(cjk_chars)))
    print('  line_height=%d, base_line=%d (ascent=%d, descent=%d)' % (line_height, base_line, ascent, descent))

if __name__ == '__main__':
    main()
