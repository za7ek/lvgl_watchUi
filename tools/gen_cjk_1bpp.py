#!/usr/bin/env python3
"""Generate LVGL 1bpp CJK font from TTF file.
Uses the exact same output format as gen_font.py (proven to work with LVGL).
1bpp = simpler than 4bpp, eliminates bitmap packing issues.

Usage:
    cd /home/zheng_fang/zephyr-project/lgvl_watchUi
    python3 tools/gen_cjk_1bpp.py
"""

import sys
from PIL import Image, ImageDraw, ImageFont

# === Configuration ===
TTF_PATH = "/usr/share/fonts/win11/simsun.ttc"
FONT_SIZE = 12
OUT_PATH = "/home/zheng_fang/zephyr-project/lgvl_watchUi/app/src/fonts/lv_font_cjk.c"
FONT_NAME = "lv_font_cjk"

# Characters to include: ASCII + CJK
ASCII_START = 0x20
ASCII_END = 0x7E

CJK_STRINGS = [
    u"周日一二三四五六",
    u"一二三四五六七八九十",
    u"正二三四五六七八九十冬腊",
    u"初十廿三",
    u"甲乙丙丁戊己庚辛壬癸",
    u"子丑寅卯辰巳午未申酉戌亥",
    u"鼠牛虎兔龙蛇马羊猴鸡狗猪",
    u"晴多云阴雨雪",
    u"心率步数电池楼层卡路里",
    u"日出落新月上弦满下",
    u"中文恢复小时周活动分钟天气温度",
    u"年月日星期",
    u"闰",
    # 24 节气用字
    u"小寒大立春水惊蛰清谷",
    u"夏满芒种至暑处白秋",
    u"露霜降",
]


def render_char_1bpp(font, ch):
    """Render a character from TTF and return 1bpp bitmap.
    Returns: (bitmap_2d, box_w, box_h, xoff, yoff, adv_w)
    bitmap_2d: rows x cols, 0=foreground, 1=background (same as gen_font.py)
    """
    ascent, descent = font.getmetrics()
    advance = font.getlength(ch)
    adv_w = int(advance)  # pixels
    if adv_w < 1:
        adv_w = 1

    margin = 4
    baseline_y = margin + ascent
    canvas_w = int(advance) + 2 * margin + 4
    canvas_h = ascent + descent + 2 * margin

    img = Image.new("L", (canvas_w, canvas_h), 0)
    draw = ImageDraw.Draw(img)
    draw.text((margin, baseline_y), ch, font=font, fill=255, anchor="ls")

    bbox = img.getbbox()
    if bbox is None:
        # No visible pixels (e.g., space)
        return None, 0, 0, 0, 0, adv_w

    x0, y0, x1, y1 = bbox
    box_w = x1 - x0
    box_h = y1 - y0
    xoff = x0 - margin
    yoff = baseline_y - y1  # offset from baseline (LVGL ofs_y = target_h - box_h - yoff)

    # Extract pixels: 0=background, 255=foreground
    # Convert to gen_font.py convention: 0=foreground, 1=background
    # IMPORTANT: simsun renders CJK strokes anti-aliased to values 60-166 (not 0/255).
    # A threshold of >128 drops the thin horizontal strokes of chars like '日' (middle bar)
    # and the diagonal of '2', producing broken/incomplete glyphs. The anti-aliasing has a
    # clean gap between the halo (val<=8) and real strokes (val>=60), so threshold=40
    # captures all strokes while rejecting background noise.
    THRESHOLD = 40
    raw_pixels = list(img.crop(bbox).getdata())
    bitmap = []
    for row in range(box_h):
        row_data = []
        for col in range(box_w):
            val = raw_pixels[row * box_w + col]
            row_data.append(0 if val > THRESHOLD else 1)  # 0=fg, 1=bg
        bitmap.append(row_data)

    return bitmap, box_w, box_h, xoff, yoff, adv_w


def bitmap_to_lvgl_1bpp(bitmap, width, height):
    """Convert bitmap to LVGL 1bpp MSB-first format.
    EXACT copy of gen_font.py's function (proven to work).
    bit=0 → foreground (render with text color)
    bit=1 → background (transparent)
    """
    data = []
    byte_val = 0
    bit_pos = 0

    for row in range(height):
        for col in range(width):
            if col < len(bitmap[row]) and row < len(bitmap):
                val = bitmap[row][col]
            else:
                val = 1  # default background
            if val == 0:
                byte_val |= (1 << (7 - bit_pos))
            bit_pos += 1
            if bit_pos == 8:
                data.append(byte_val)
                byte_val = 0
                bit_pos = 0

    if bit_pos > 0:
        data.append(byte_val)

    return bytes(data)


def main():
    # Build character list
    ascii_chars = [chr(c) for c in range(ASCII_START, ASCII_END + 1)]
    cjk_set = set()
    for s in CJK_STRINGS:
        for ch in s:
            cjk_set.add(ch)
    cjk_chars = sorted(cjk_set, key=ord)

    all_chars = ascii_chars + cjk_chars
    print(f"Total characters: {len(all_chars)} ({len(ascii_chars)} ASCII + {len(cjk_chars)} CJK)")

    # Load font
    font = ImageFont.truetype(TTF_PATH, FONT_SIZE)
    ascent, descent = font.getmetrics()
    target_h = ascent + descent
    print(f"Font: {TTF_PATH}, size={FONT_SIZE}, ascent={ascent}, descent={descent}, line_height={target_h}")

    # Render all glyphs
    glyph_dscs = []
    bitmap_data = bytearray()
    actual_ids = []

    # Prepend reserved dummy entry at glyph_dsc[0] (same as gen_font.py)
    reserved_dsc = {
        'bitmap_index': 0, 'adv_w': 0, 'box_w': 0, 'box_h': 0, 'ofs_x': 0, 'ofs_y': 0
    }
    glyph_dscs.append(reserved_dsc)

    for ch in all_chars:
        cid = ord(ch)
        bitmap, box_w, box_h, xoff, yoff, adv_px = render_char_1bpp(font, ch)

        if cid == 32:  # space: zero-size, no bitmap
            box_w = 0
            box_h = 0
            data = b''
            lvgl_ofs_y = target_h
        elif bitmap is None:
            data = b''
            lvgl_ofs_y = target_h
        else:
            data = bitmap_to_lvgl_1bpp(bitmap, box_w, box_h)
            lvgl_ofs_y = target_h - box_h - yoff

        glyph_dsc = {
            'bitmap_index': len(bitmap_data),
            'adv_w': adv_px * 16,  # convert to 1/16 pixel units
            'box_w': box_w,
            'box_h': box_h,
            'ofs_x': xoff,
            'ofs_y': lvgl_ofs_y
        }
        glyph_dscs.append(glyph_dsc)
        bitmap_data.extend(data)
        actual_ids.append(cid)

    print(f"Total bitmap data: {len(bitmap_data)} bytes")
    print(f"Total glyphs: {len(glyph_dscs)} (1 reserved + {len(actual_ids)} chars)")

    # Build cmap entries: glyph_id_start = i + 1 (same as gen_font.py)
    cmap_entries = []
    for i, cid in enumerate(actual_ids):
        cmap_entries.append({
            'range_start': cid,
            'range_length': 1,
            'glyph_id_start': i + 1
        })

    # Generate output file (same format as gen_font.py)
    with open(OUT_PATH, 'w') as f:
        f.write('/*\n')
        f.write(f' * Auto-generated LVGL 1bpp CJK font: {FONT_NAME}\n')
        f.write(f' * Source: {TTF_PATH}\n')
        f.write(f' * Font size: {FONT_SIZE}px, line_height: {target_h}\n')
        f.write(f' * Characters: {len(actual_ids)} ({len(ascii_chars)} ASCII + {len(cjk_chars)} CJK)\n')
        f.write(' */\n\n')
        f.write('#include "lv_font_cjk.h"\n\n')

        # Bitmap data
        f.write('/*Store the image of the glyphs*/\n')
        f.write('static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {\n')
        for i in range(0, len(bitmap_data), 16):
            chunk = bitmap_data[i:i+16]
            line = '    ' + ', '.join(f'0x{b:02X}' for b in chunk)
            if i + 16 < len(bitmap_data):
                line += ','
            f.write(line + '\n')
        f.write('};\n\n')

        # Glyph descriptors
        f.write('/*Describe the properties of all glyphs*/\n')
        f.write('static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {\n')
        for i, gd in enumerate(glyph_dscs):
            comma = ',' if i < len(glyph_dscs) - 1 else ''
            if i == 0:
                f.write(f'    {{ {gd["bitmap_index"]:>5d}, {gd["adv_w"]:>4d}, {gd["box_w"]:>3d}, {gd["box_h"]:>3d}, {gd["ofs_x"]:>2d}, {gd["ofs_y"]:>2d} }}  /* reserved dummy */{comma}\n')
            else:
                cid = actual_ids[i - 1]
                if cid >= 0x4E00:
                    f.write(f'    {{ {gd["bitmap_index"]:>5d}, {gd["adv_w"]:>4d}, {gd["box_w"]:>3d}, {gd["box_h"]:>3d}, {gd["ofs_x"]:>2d}, {gd["ofs_y"]:>2d} }}  /* U+{cid:04X} */{comma}\n')
                else:
                    f.write(f'    {{ {gd["bitmap_index"]:>5d}, {gd["adv_w"]:>4d}, {gd["box_w"]:>3d}, {gd["box_h"]:>3d}, {gd["ofs_x"]:>2d}, {gd["ofs_y"]:>2d} }}  /* 0x{cid:02X} */{comma}\n')
        f.write('};\n\n')

        # Cmap entries
        f.write('/*Collect the unicode lists and glyph_id offsets*/\n')
        f.write(f'static const lv_font_fmt_txt_cmap_t cmaps[] = {{\n')
        for i, entry in enumerate(cmap_entries):
            comma = ',' if i < len(cmap_entries) - 1 else ''
            f.write('    {\n')
            f.write(f'        .range_start = {entry["range_start"]}, .range_length = {entry["range_length"]}, .glyph_id_start = {entry["glyph_id_start"]},\n')
            f.write('        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY\n')
            f.write('    }' + comma + '\n')
        f.write('};\n\n')

        # Font descriptor
        f.write('/*Store all the custom data of the font*/\n')
        f.write('static const lv_font_fmt_txt_dsc_t font_dsc = {\n')
        f.write('    .glyph_bitmap = glyph_bitmap,\n')
        f.write('    .glyph_dsc = glyph_dsc,\n')
        f.write('    .cmaps = cmaps,\n')
        f.write('    .kern_dsc = NULL,\n')
        f.write('    .kern_scale = 0,\n')
        f.write(f'    .cmap_num = {len(cmap_entries)},\n')
        f.write('    .bpp = 1,\n')
        f.write('    .kern_classes = 0,\n')
        f.write('    .bitmap_format = 0,\n')
        f.write('};\n\n')

        # Font variable
        f.write('/*Initialize a public general font descriptor*/\n')
        f.write(f'const lv_font_t {FONT_NAME} = {{\n')
        f.write('    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,\n')
        f.write('    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,\n')
        f.write(f'    .line_height = {target_h},\n')
        f.write('    .base_line = 0,\n')
        f.write('    .subpx = LV_FONT_SUBPX_NONE,\n')
        f.write('    .underline_position = 0,\n')
        f.write('    .underline_thickness = 0,\n')
        f.write('    .dsc = &font_dsc\n')
        f.write('};\n')

    print(f"\nGenerated: {OUT_PATH}")
    print(f"Font variable: {FONT_NAME}")
    print(f"cmap_num: {len(cmap_entries)}")
    print(f"glyph count: {len(glyph_dscs)}")
    print(f"bpp: 1")
    print(f"line_height: {target_h}")


if __name__ == '__main__':
    main()
