#!/usr/bin/env python3
"""Generate LVGL 9.x font from BMFont FNT + PNG files.
General-purpose: handles variable-width characters, any font size.

Usage:
    python3 gen_font.py <fnt_path> <png_path> <output_path> <font_name> [char_ids...]
    
    If no char_ids specified, generates all chars found in FNT.
    Char IDs can be decimal (65) or hex (0x41).
"""

import sys
from PIL import Image


def parse_fnt(fnt_path):
    """Parse BMFont .fnt file, return dict of char_id -> metrics."""
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
                if char_id is not None:
                    chars[char_id] = {
                        'id': char_id, 'x': x, 'y': y, 'w': w, 'h': h,
                        'xoff': xoff, 'yoff': yoff, 'xadv': xadv, 'page': page
                    }
    return chars


def detect_dark_background(img):
    """Auto-detect if the PNG has a dark background (white text on black).
    Checks the corner pixel which is typically background.
    Returns True if background is dark (text is light), False otherwise.
    """
    gray = img.convert('L')
    corner = gray.getpixel((0, 0))
    # Also check a few edge pixels for robustness
    w, h = gray.size
    samples = [corner]
    if w > 1: samples.append(gray.getpixel((w-1, 0)))
    if h > 1: samples.append(gray.getpixel((0, h-1)))
    if w > 1 and h > 1: samples.append(gray.getpixel((w-1, h-1)))
    avg = sum(samples) / len(samples)
    return avg < 128


def extract_char_bitmap(img, char_info, target_h, dark_bg=False):
    """Extract character bitmap from PNG.
    
    Returns a 2D list: rows x cols, where 0=foreground, 1=background.
    The bitmap is padded to target_h height and the actual char width.
    dark_bg: if True, the PNG has dark background with light text (inverted polarity).
    """
    x = char_info['x']
    y = char_info['y']
    w = char_info['w']
    h = char_info['h']
    xoff = char_info.get('xoff', 0)
    yoff = char_info.get('yoff', 0)
    
    img_gray = img.convert('L')
    raw = img_gray.crop((x, y, x + w, y + h))
    raw_pixels = list(raw.getdata())
    
    bitmap = [[1] * w for _ in range(h)]
    
    for row in range(h):
        for col in range(w):
            pixel_val = raw_pixels[row * w + col]
            if dark_bg:
                # Dark background: light pixels are foreground (text)
                is_fg = pixel_val > 128
            else:
                # Light background: dark pixels are foreground (text)
                is_fg = pixel_val < 128
            bitmap[row][col] = 0 if is_fg else 1
    
    return bitmap


def bitmap_to_lvgl_1bpp(bitmap, width, height):
    """Convert bitmap (rows x cols) to LVGL 1bpp MSB-first format.
    LVGL reads bits continuously (no per-row padding).
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


def create_empty_bitmap(w, h):
    """Create an all-background bitmap."""
    return [[1] * w for _ in range(h)]


def parse_id_list(id_str):
    """Parse a character ID string (decimal or hex)."""
    id_str = id_str.strip()
    if id_str.startswith('0x') or id_str.startswith('0X'):
        return int(id_str, 16)
    return int(id_str)


def get_char_display_name(cid):
    """Get a display name for a character ID."""
    if cid == 32:
        return 'space'
    elif cid == 9:
        return 'tab'
    elif cid == 10:
        return 'newline'
    elif cid >= 33 and cid <= 126:
        return repr(chr(cid))
    elif cid >= 0x4E00 and cid <= 0x9FFF:
        return f'U+{cid:04X}'
    else:
        return f'0x{cid:04X}'


def main():
    if len(sys.argv) < 5:
        print(f"Usage: {sys.argv[0]} <fnt_path> <png_path> <output_path> <font_name> [char_ids...]")
        sys.exit(1)
    
    fnt_path = sys.argv[1]
    png_path = sys.argv[2]
    output_path = sys.argv[3]
    font_name = sys.argv[4]
    font_var = f'lv_font_{font_name}'
    
    chars = parse_fnt(fnt_path)
    print(f"Parsed {len(chars)} characters from FNT")
    
    img = Image.open(png_path)
    img_w, img_h = img.size
    print(f"Image size: {img.size}")
    
    # Auto-detect background polarity (dark bg with light text, or light bg with dark text)
    dark_bg = detect_dark_background(img)
    print(f"Background polarity: {'dark bg / light text (inverted)' if dark_bg else 'light bg / dark text (normal)'}")
    
    # Determine target height from FNT
    with open(fnt_path, 'r') as f:
        for line in f:
            if line.startswith('common lineHeight='):
                target_h = int(line.split('lineHeight=')[1].split()[0])
                break
        else:
            target_h = 13
    
    # Determine which chars to generate
    if len(sys.argv) > 5:
        requested_ids = [parse_id_list(x) for x in sys.argv[5:]]
    else:
        requested_ids = sorted(chars.keys())
    
    print(f"Target height: {target_h}px")
    print(f"Requesting {len(requested_ids)} characters")
    
    glyph_dscs = []
    bitmap_data = bytearray()
    actual_ids = []
    
    for cid in requested_ids:
        name = get_char_display_name(cid)
        
        if cid not in chars:
            print(f"  SKIP: char {cid} ({name}) not found in FNT")
            continue
        
        info = chars[cid]
        bitmap = extract_char_bitmap(img, info, target_h, dark_bg)
        
        box_w = info['w']
        box_h = info['h']
        xadv = info['xadv']
        xoff = info['xoff']
        yoff = info['yoff']
        
        # Special handling for space (char 32): zero-size, no bitmap
        if cid == 32:
            box_w = 0
            box_h = 0
            data = b''
            lvgl_ofs_y = target_h
        else:
            data = bitmap_to_lvgl_1bpp(bitmap, box_w, box_h)
            lvgl_ofs_y = target_h - box_h - yoff
        
        glyph_dsc = {
            'bitmap_index': len(bitmap_data),
            'adv_w': xadv * 16,
            'box_w': box_w,
            'box_h': box_h,
            'ofs_x': xoff,
            'ofs_y': lvgl_ofs_y
        }
        glyph_dscs.append(glyph_dsc)
        bitmap_data.extend(data)
        actual_ids.append(cid)
        
        print(f"  Char '{name}' (id={cid}): {box_w}x{box_h}, {len(data)} bytes, adv_w={glyph_dsc['adv_w']}, ofs_x={xoff}, ofs_y={lvgl_ofs_y} (bm_yoffset={yoff})")
        
        if box_w > 0 and box_h > 0:
            bitmap_preview = []
            for r in range(box_h):
                row_str = ''.join(['#' if bitmap[r][c] == 0 else '.' for c in range(box_w)])
                bitmap_preview.append(row_str)
            for r in bitmap_preview:
                print(f"    {r}")
    
    print(f"Total bitmap data size: {len(bitmap_data)} bytes")
    
    if not glyph_dscs:
        print("ERROR: No glyphs generated!")
        sys.exit(1)
    
    # Prepend a reserved dummy entry at glyph_dsc[0]
    # All real glyphs are shifted by +1
    reserved_dsc = {
        'bitmap_index': 0,
        'adv_w': 0,
        'box_w': 0,
        'box_h': 0,
        'ofs_x': 0,
        'ofs_y': 0
    }
    glyph_dscs.insert(0, reserved_dsc)
    
    # Build cmap entries: glyph_id_start = i + 1 to account for reserved entry
    cmap_entries = []
    for i, cid in enumerate(actual_ids):
        cmap_entries.append({
            'range_start': cid,
            'range_length': 1,
            'glyph_id_start': i + 1
        })
    
    # Generate output file
    with open(output_path, 'w') as f:
        f.write('/*\n')
        f.write(f' * Auto-generated LVGL 9.x font: {font_name}\n')
        f.write(f' * Source: {fnt_path} + {png_path}\n')
        f.write(f' * Image size: {img_w}x{img_h}\n')
        f.write(f' * Font height: {target_h}px\n')
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
            if i == 0:
                f.write(f'    {{ {gd["bitmap_index"]:>5d}, {gd["adv_w"]:>4d}, {gd["box_w"]:>3d}, {gd["box_h"]:>3d}, {gd["ofs_x"]:>2d}, {gd["ofs_y"]:>2d} }}  /* reserved dummy */{comma}\n')
            else:
                cid = actual_ids[i - 1]
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
        f.write(f'const lv_font_t {font_var} = {{\n')
        f.write('    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,\n')
        f.write('    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,\n')
        f.write(f'    .line_height = {target_h},\n')
        f.write('    .base_line = 0,\n')
        f.write('    .subpx = LV_FONT_SUBPX_NONE,\n')
        f.write('    .underline_position = 0,\n')
        f.write('    .underline_thickness = 0,\n')
        f.write('    .dsc = &font_dsc\n')
        f.write('};\n')
    
    print(f"\nGenerated font file: {output_path}")
    print(f"Font variable: {font_var}")
    print(f"cmap_num: {len(cmap_entries)}")
    print(f"glyph count: {len(glyph_dscs)}")


if __name__ == '__main__':
    main()