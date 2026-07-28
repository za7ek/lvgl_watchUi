#!/usr/bin/env python3
"""Generate LVGL 9.x LED font from BMFont led.fnt and led.png
Direct pixel extraction from PNG, preserving original patterns exactly.
Characters are 14x20 pixels with 5x7 LED block structure (2x2px blocks, 1px gaps).
"""

import sys
from PIL import Image

BLOCK_COLS = 5
BLOCK_ROWS = 7
BLOCK_SIZE = 2
BLOCK_GAP = 1
CHAR_W = BLOCK_COLS * BLOCK_SIZE + (BLOCK_COLS - 1) * BLOCK_GAP  # 14
CHAR_H = BLOCK_ROWS * BLOCK_SIZE + (BLOCK_ROWS - 1) * BLOCK_GAP  # 20

def parse_fnt(fnt_path):
    chars = {}
    with open(fnt_path, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('char id='):
                parts = line.split()
                char_id = None
                x = y = w = h = xoff = yoff = xadv = page = 0
                for part in parts:
                    if part.startswith('id='):
                        char_id = int(part[3:])
                    elif part.startswith('x='):
                        x = int(part[2:])
                    elif part.startswith('y='):
                        y = int(part[2:])
                    elif part.startswith('width='):
                        w = int(part[6:])
                    elif part.startswith('height='):
                        h = int(part[7:])
                    elif part.startswith('xoffset='):
                        xoff = int(part[8:])
                    elif part.startswith('yoffset='):
                        yoff = int(part[8:])
                    elif part.startswith('xadvance='):
                        xadv = int(part[9:])
                    elif part.startswith('page='):
                        page = int(part[5:])
                if char_id is not None and w > 0 and h > 0:
                    chars[char_id] = {
                        'id': char_id, 'x': x, 'y': y, 'w': w, 'h': h,
                        'xoff': xoff, 'yoff': yoff, 'xadv': xadv, 'page': page
                    }
    return chars


def extract_bitmap_14x20(img, char_info):
    """Extract character bitmap from PNG and pad to 14x20.
    Direct pixel extraction - preserves the original pixel pattern exactly.
    Returns a 2D array: 0=foreground (lit/dark pixel), 1=background (gap/light pixel)
    """
    x, y, w, h = char_info['x'], char_info['y'], char_info['w'], char_info['h']
    xoff = char_info.get('xoff', 0)
    yoff = char_info.get('yoff', 0)

    img_gray = img.convert('L')

    raw = img_gray.crop((x, y, x + w, y + h))
    raw_pixels = list(raw.getdata())

    bitmap = [[1] * CHAR_W for _ in range(CHAR_H)]

    for row in range(h):
        for col in range(w):
            pixel_val = raw_pixels[row * w + col]
            is_lit = 1 if pixel_val < 128 else 0
            dest_x = xoff + col
            dest_y = yoff + row
            if 0 <= dest_x < CHAR_W and 0 <= dest_y < CHAR_H:
                if is_lit:
                    bitmap[dest_y][dest_x] = 0
                else:
                    bitmap[dest_y][dest_x] = 1

    return bitmap


def create_space_bitmap():
    """Space character - all transparent"""
    return [[1] * CHAR_W for _ in range(CHAR_H)]


def create_dot_bitmap():
    """Decimal point: 4 blocks at cols 1-2, rows 5-6 (0-indexed).
    This creates a 2x2 block pattern in the bottom-right area.
    """
    bitmap = [[1] * CHAR_W for _ in range(CHAR_H)]

    for block_row in [5, 6]:
        for block_col in [1, 2]:
            start_x = block_col * (BLOCK_SIZE + BLOCK_GAP)
            start_y = block_row * (BLOCK_SIZE + BLOCK_GAP)
            for dy in range(BLOCK_SIZE):
                for dx in range(BLOCK_SIZE):
                    px = start_x + dx
                    py = start_y + dy
                    if 0 <= px < CHAR_W and 0 <= py < CHAR_H:
                        bitmap[py][px] = 0

    return bitmap


def create_hash_bitmap():
    """'#' character - all 35 LED blocks lit (creates the dot grid background).
    Each block is 2x2 pixels, 1px gap between blocks.
    """
    bitmap = [[1] * CHAR_W for _ in range(CHAR_H)]

    for block_row in range(BLOCK_ROWS):
        for block_col in range(BLOCK_COLS):
            start_x = block_col * (BLOCK_SIZE + BLOCK_GAP)
            start_y = block_row * (BLOCK_SIZE + BLOCK_GAP)
            for dy in range(BLOCK_SIZE):
                for dx in range(BLOCK_SIZE):
                    px = start_x + dx
                    py = start_y + dy
                    if 0 <= px < CHAR_W and 0 <= py < CHAR_H:
                        bitmap[py][px] = 0

    return bitmap


def bitmap_to_lvgl(bitmap):
    """Convert 14x20 bitmap to LVGL 1bpp format.
    LVGL 1bpp (bitmap_format=0): MSB-first within each byte.
    bit=0 → foreground (render with text color)
    bit=1 → background (transparent)
    Input bitmap: 0=foreground (lit), 1=background (gap)
    """
    data = []
    byte_val = 0
    bit_pos = 0

    for row in range(CHAR_H):
        for col in range(CHAR_W):
            val = bitmap[row][col]
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
    fnt_path = '/home/zheng_fang/zephyr-project/Segment34.CN/resources/fonts/led.fnt'
    png_path = '/home/zheng_fang/zephyr-project/Segment34.CN/resources/fonts/led.png'
    output_path = '/home/zheng_fang/zephyr-project/lgvl_watchUi/app/src/fonts/lv_font_led.c'

    chars = parse_fnt(fnt_path)
    print(f"Parsed {len(chars)} characters from FNT")

    img = Image.open(png_path)
    print(f"Image size: {img.size}")

    needed_ids = [32, 35, 46, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58]
    display_names = {
        32: 'space', 35: '#', 46: '.',
        48: '0', 49: '1', 50: '2', 51: '3', 52: '4',
        53: '5', 54: '6', 55: '7', 56: '8', 57: '9', 58: ':'
    }

    glyph_dscs = []
    bitmap_data = bytearray()

    for glyph_idx, cid in enumerate(needed_ids):
        name = display_names.get(cid, f'?{cid}')

        if cid == 32:
            bitmap = create_space_bitmap()
            xadv = 18
        elif cid == 35:
            bitmap = create_hash_bitmap()
            xadv = 18
        elif cid == 46:
            bitmap = create_dot_bitmap()
            xadv = 18
        elif cid == 58 and cid in chars:
            info = chars[cid]
            bitmap = extract_bitmap_14x20(img, info)
            xadv = info['xadv']
        elif cid in chars:
            info = chars[cid]
            bitmap = extract_bitmap_14x20(img, info)
            xadv = info['xadv']
        else:
            print(f"  WARNING: char {cid} ({name}) not found in FNT, creating empty")
            bitmap = create_space_bitmap()
            xadv = 18

        data = bitmap_to_lvgl(bitmap)

        ofs_x = xadv - CHAR_W  # right-align within cell (xadv=18, CHAR_W=14 → ofs_x=4)
        glyph_dsc = {
            'bitmap_index': len(bitmap_data),
            'adv_w': xadv * 16,
            'box_w': CHAR_W,
            'box_h': CHAR_H,
            'ofs_x': ofs_x,
            'ofs_y': 0
        }
        glyph_dscs.append(glyph_dsc)
        bitmap_data.extend(data)
        print(f"  Char '{name}' (id={cid}): {CHAR_W}x{CHAR_H}, {len(data)} bytes, offset={glyph_dsc['bitmap_index']}")

        bitmap_preview = []
        for r in range(CHAR_H):
            row_str = ''.join(['#' if bitmap[r][c] == 0 else '.' for c in range(CHAR_W)])
            bitmap_preview.append(row_str)
        for r in bitmap_preview:
            print(f"    {r}")

    print(f"Total bitmap data size: {len(bitmap_data)} bytes")

    cmap_entries = []
    for i, cid in enumerate(needed_ids):
        cmap_entries.append({
            'range_start': cid,
            'range_length': 1,
            'glyph_id_start': i
        })

    with open(output_path, 'w') as f:
        f.write('/*\n')
        f.write(' * Auto-generated LVGL 9.x LED font\n')
        f.write(' * Source: Segment34.CN/resources/fonts/led.fnt + led.png\n')
        f.write(' * Direct pixel extraction from PNG (preserves original patterns)\n')
        f.write(' * Each character is 5x7 LED blocks (14x20 pixels)\n')
        f.write(' */\n\n')

        f.write('#include "lvgl.h"\n\n')

        f.write('/*Store the image of the glyphs*/\n')
        f.write('static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {\n')
        for i in range(0, len(bitmap_data), 16):
            chunk = bitmap_data[i:i+16]
            line = '    ' + ', '.join(f'0x{b:02X}' for b in chunk)
            if i + 16 < len(bitmap_data):
                line += ','
            f.write(line + '\n')
        f.write('};\n\n')

        f.write('/*Describe the properties of all glyphs*/\n')
        f.write('static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {\n')
        for i, gd in enumerate(glyph_dscs):
            comma = ',' if i < len(glyph_dscs) - 1 else ''
            cid = needed_ids[i]
            f.write(f'    {{ {gd["bitmap_index"]:>5d}, {gd["adv_w"]:>4d}, {gd["box_w"]:>3d}, {gd["box_h"]:>3d}, {gd["ofs_x"]:>2d}, {gd["ofs_y"]:>2d} }}  /* 0x{cid:04X} */{comma}\n')
        f.write('};\n\n')

        f.write('/*Collect the unicode lists and glyph_id offsets*/\n')
        f.write(f'static const lv_font_fmt_txt_cmap_t cmaps[] = {{\n')
        for i, entry in enumerate(cmap_entries):
            comma = ',' if i < len(cmap_entries) - 1 else ''
            f.write('    {\n')
            f.write(f'        .range_start = {entry["range_start"]}, .range_length = {entry["range_length"]}, .glyph_id_start = {entry["glyph_id_start"]},\n')
            f.write('        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY\n')
            f.write('    }' + comma + '\n')
        f.write('};\n\n')

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

        f.write('/*Initialize a public general font descriptor*/\n')
        f.write('const lv_font_t lv_font_led = {\n')
        f.write('    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,\n')
        f.write('    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,\n')
        f.write('    .line_height = 20,\n')
        f.write('    .base_line = 0,\n')
        f.write('    .subpx = LV_FONT_SUBPX_NONE,\n')
        f.write('    .underline_position = 0,\n')
        f.write('    .underline_thickness = 0,\n')
        f.write('    .dsc = &font_dsc\n')
        f.write('};\n')

    print(f"\nGenerated font file: {output_path}")
    print(f"cmap_num: {len(cmap_entries)}")
    print(f"glyph count: {len(glyph_dscs)}")


if __name__ == '__main__':
    main()
