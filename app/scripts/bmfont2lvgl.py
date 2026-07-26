#!/usr/bin/env python3
"""
Convert BMFont (.fnt + .png) to LVGL C array font format.
Usage: python bmfont2lvgl.py <font_name> <fnt_file> <png_file> <output_c_file>
"""

import sys
import os
import struct
from PIL import Image

def parse_fnt(fnt_path):
    chars = {}
    common = {}
    with open(fnt_path, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('common '):
                parts = line.split()
                for p in parts[1:]:
                    k, v = p.split('=')
                    if k == 'lineHeight': common['lineHeight'] = int(v)
                    elif k == 'base': common['base'] = int(v)
                    elif k == 'scaleW': common['scaleW'] = int(v)
                    elif k == 'scaleH': common['scaleH'] = int(v)
                    elif k == 'pages': common['pages'] = int(v)
            elif line.startswith('char '):
                parts = line.split()
                cid = 0
                cdata = {}
                for p in parts[1:]:
                    k, v = p.split('=')
                    if k == 'id': cid = int(v)
                    elif k == 'x': cdata['x'] = int(v)
                    elif k == 'y': cdata['y'] = int(v)
                    elif k == 'width': cdata['w'] = int(v)
                    elif k == 'height': cdata['h'] = int(v)
                    elif k == 'xoffset': cdata['xoffset'] = int(v)
                    elif k == 'yoffset': cdata['yoffset'] = int(v)
                    elif k == 'xadvance': cdata['xadvance'] = int(v)
                chars[cid] = cdata
    return common, chars

def extract_glyph_bmp(img, x, y, w, h):
    """Extract alpha values from a glyph region as 4bpp."""
    glyph = []
    for py in range(y, y + h):
        row = []
        for px in range(x, x + w):
            if px < img.width and py < img.height:
                pixel = img.getpixel((px, py))
                if isinstance(pixel, tuple):
                    if len(pixel) > 3:
                        alpha = pixel[3]
                    else:
                        alpha = (pixel[0] + pixel[1] + pixel[2]) // 3
                else:
                    alpha = pixel
                alpha = 255 - alpha
            else:
                alpha = 0
            row.append(alpha)
        glyph.append(row)
    return glyph

def bmp_to_4bpp(glyph):
    """Convert 8-bit alpha glyph to 4bpp packed data."""
    data = []
    for row in glyph:
        for i in range(0, len(row), 2):
            hi = (row[i] >> 4) & 0x0F if i < len(row) else 0
            lo = (row[i+1] >> 4) & 0x0F if i+1 < len(row) else 0
            data.append((hi << 4) | lo)
    return data

def generate_c_font(font_name, fnt_path, png_path, out_path):
    common, chars = parse_fnt(fnt_path)
    img = Image.open(png_path)
    
    line_height = common['lineHeight']
    base = common['base']
    # LVGL base_line is descent (distance from baseline to bottom of line)
    # BMFont base is ascent (distance from top of line to baseline)
    base_line = line_height - base
    
    sorted_cids = sorted(chars.keys())
    first_char = sorted_cids[0]
    last_char = sorted_cids[-1]
    char_count = len(sorted_cids)
    
    glyph_data = []
    glyph_dsc = []
    
    for cid in sorted_cids:
        c = chars[cid]
        w = c['w']
        h = c['h']
        xoffset = c['xoffset']
        yoffset = c['yoffset']
        xadvance = c['xadvance']
        
        # Convert BMFont offsets to LVGL offsets:
        # BMFont yoffset: from top of line to top of glyph
        # BMFont base: from top of line to baseline (ascent)
        # LVGL ofs_y: baseline_y - glyph_bottom_y (glyph bottom above baseline = positive)
        #   glyph_bottom_y = yoffset + h (from top of line)
        #   baseline_y = base (from top of line)
        #   ofs_y = base - (yoffset + h) = base - yoffset - h
        ofs_y = base - yoffset - h
        
        if w == 0 or h == 0:
            glyph_dsc.append({
                'unicode': cid,
                'w': 0, 'h': 0,
                'ofs_x': 0, 'ofs_y': 0,
                'xadvance': xadvance,
                'data_offset': 0,
            })
            continue
        
        glyph_bmp = extract_glyph_bmp(img, c['x'], c['y'], w, h)
        bpp4_data = bmp_to_4bpp(glyph_bmp)
        
        data_offset = len(glyph_data)
        glyph_data.extend(bpp4_data)
        
        glyph_dsc.append({
            'unicode': cid,
            'w': w, 'h': h,
            'ofs_x': xoffset, 'ofs_y': ofs_y,
            'xadvance': xadvance,
            'data_offset': data_offset,
        })
    
    # Pad glyph_data to 4-byte alignment
    while len(glyph_data) % 4 != 0:
        glyph_data.append(0)
    
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(f"/* Auto-generated LVGL font from BMFont */\n")
        f.write(f"/* Source: {os.path.basename(fnt_path)} + {os.path.basename(png_path)} */\n")
        f.write(f"#include \"lvgl.h\"\n\n")
        
        # Glyph bitmap data
        f.write(f"static const uint8_t {font_name}_glyph_bitmap[] = {{\n")
        for i in range(0, len(glyph_data), 16):
            chunk = glyph_data[i:i+16]
            hex_str = ', '.join(f'0x{b:02X}' for b in chunk)
            f.write(f"    {hex_str},\n")
        f.write(f"}};\n\n")
        
        # Glyph descriptors: order is bitmap_index, adv_w, box_w, box_h, ofs_x, ofs_y
        # adv_w is in 1/16 pixel units
        f.write(f"static const lv_font_fmt_txt_glyph_dsc_t {font_name}_glyph_dsc[] = {{\n")
        for g in glyph_dsc:
            adv_w = g['xadvance'] * 16
            entry = "    {" + f"{g['data_offset']:>5}, {adv_w:>5}, {g['w']:>3}, {g['h']:>3}, {g['ofs_x']:>4}, {g['ofs_y']:>4}" + "},  /* 0x%04X */\n" % g['unicode']
            f.write(entry)
        f.write(f"}};\n\n")
        
        # CMAP (format 4 - sparse)
        f.write(f"static const uint16_t {font_name}_unicode_list[] = {{\n")
        for i, g in enumerate(glyph_dsc):
            comma = ',' if i < len(glyph_dsc) - 1 else ''
            offset_val = g['unicode'] - first_char
            f.write(f"    0x{offset_val:04X}{comma}\n")
        f.write(f"}};\n\n")
        
        f.write(f"static const lv_font_fmt_txt_cmap_t {font_name}_cmaps[] = {{\n")
        f.write(f"    {{\n")
        f.write(f"        .range_start = 0x{first_char:04X},\n")
        f.write(f"        .range_length = {last_char - first_char + 1},\n")
        f.write(f"        .glyph_id_start = 0,\n")
        f.write(f"        .unicode_list = {font_name}_unicode_list,\n")
        f.write(f"        .glyph_id_ofs_list = NULL,\n")
        f.write(f"        .list_length = {char_count},\n")
        f.write(f"        .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY,\n")
        f.write(f"    }}\n")
        f.write(f"}};\n\n")
        
        # Font dsc
        f.write(f"static const lv_font_fmt_txt_dsc_t {font_name}_font_dsc = {{\n")
        f.write(f"    .glyph_bitmap = {font_name}_glyph_bitmap,\n")
        f.write(f"    .glyph_dsc = {font_name}_glyph_dsc,\n")
        f.write(f"    .cmaps = {font_name}_cmaps,\n")
        f.write(f"    .kern_classes = 0,\n")
        f.write(f"    .kern_dsc = NULL,\n")
        f.write(f"    .kern_scale = 0,\n")
        f.write(f"    .cmap_num = 1,\n")
        f.write(f"    .bpp = 4,\n")
        f.write(f"    .bitmap_format = 0,\n")
        f.write(f"}};\n\n")
        
        # Font struct
        f.write(f"const lv_font_t {font_name} = {{\n")
        f.write(f"    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,\n")
        f.write(f"    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,\n")
        f.write(f"    .line_height = {line_height},\n")
        f.write(f"    .base_line = {base_line},\n")
        f.write(f"    .subpx = LV_FONT_SUBPX_NONE,\n")
        f.write(f"    .underline_position = -1,\n")
        f.write(f"    .underline_thickness = 0,\n")
        f.write(f"    .dsc = &{font_name}_font_dsc,\n")
        f.write(f"}};\n\n")
    
    print(f"Generated: {out_path}")
    print(f"  Line height: {line_height}")
    print(f"  Base: {base}")
    print(f"  Char count: {char_count}")
    print(f"  Glyph data size: {len(glyph_data)} bytes")

if __name__ == '__main__':
    if len(sys.argv) != 5:
        print("Usage: python bmfont2lvgl.py <font_name> <fnt_file> <png_file> <output_c_file>")
        sys.exit(1)
    
    font_name = sys.argv[1]
    fnt_path = sys.argv[2]
    png_path = sys.argv[3]
    out_path = sys.argv[4]
    
    generate_c_font(font_name, fnt_path, png_path, out_path)
