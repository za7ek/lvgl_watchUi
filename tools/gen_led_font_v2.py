#!/usr/bin/env python3
"""Generate LVGL 9.x font from BMFont led.fnt and led.png
Converts 14x20 pixel bitmaps to 5x7 LED block structure with proper gaps
"""

import sys
from PIL import Image

# LED block configuration
BLOCK_COLS = 5
BLOCK_ROWS = 7
BLOCK_SIZE = 2  # Each LED block is 2x2 pixels
BLOCK_GAP = 1   # Gap between blocks

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

def extract_bitmap(img, char_info):
    """Extract raw bitmap from PNG and pad to target size"""
    x, y, w, h = char_info['x'], char_info['y'], char_info['w'], char_info['h']
    
    char_img = img.crop((x, y, x + w, y + h))
    char_img = char_img.convert('L')
    pixels = list(char_img.getdata())
    
    # Target size: 5 cols × 2px + 4 gaps × 1px = 14, 7 rows × 2px + 6 gaps × 1px = 20
    target_w = BLOCK_COLS * BLOCK_SIZE + (BLOCK_COLS - 1) * BLOCK_GAP  # 14
    target_h = BLOCK_ROWS * BLOCK_SIZE + (BLOCK_ROWS - 1) * BLOCK_GAP  # 20
    
    # Create target bitmap
    bitmap = [[0] * target_w for _ in range(target_h)]
    
    # Calculate offset to center the character
    offset_x = (target_w - w) // 2
    offset_y = (target_h - h) // 2
    
    # Copy pixels to target bitmap
    for row in range(h):
        for col in range(w):
            idx = row * w + col
            pixel = pixels[idx]
            bit = 1 if pixel < 128 else 0
            # Place at offset position
            target_row = row + offset_y
            target_col = col + offset_x
            if 0 <= target_row < target_h and 0 <= target_col < target_w:
                bitmap[target_row][target_col] = bit
    
    return bitmap

def bitmap_to_block_map(bitmap):
    """Convert raw bitmap to 5x7 block map"""
    h = len(bitmap)
    w = len(bitmap[0]) if h > 0 else 0
    
    block_map = []
    for block_row in range(BLOCK_ROWS):
        row_blocks = []
        for block_col in range(BLOCK_COLS):
            # Calculate pixel range for this block
            start_x = block_col * (BLOCK_SIZE + BLOCK_GAP)
            start_y = block_row * (BLOCK_SIZE + BLOCK_GAP)
            
            is_lit = False
            for dy in range(BLOCK_SIZE):
                for dx in range(BLOCK_SIZE):
                    px = start_x + dx
                    py = start_y + dy
                    if px < w and py < h:
                        if bitmap[py][px]:
                            is_lit = True
                            break
                if is_lit:
                    break
            
            row_blocks.append(is_lit)
        block_map.append(row_blocks)
    
    return block_map

def block_map_to_bitmap(block_map):
    """Convert 5x7 block map back to 14x20 pixel bitmap with proper gaps"""
    target_w = BLOCK_COLS * BLOCK_SIZE + (BLOCK_COLS - 1) * BLOCK_GAP  # 14
    target_h = BLOCK_ROWS * BLOCK_SIZE + (BLOCK_ROWS - 1) * BLOCK_GAP  # 20
    
    bitmap = [[0] * target_w for _ in range(target_h)]
    
    for block_row in range(BLOCK_ROWS):
        for block_col in range(BLOCK_COLS):
            if block_map[block_row][block_col]:
                start_x = block_col * (BLOCK_SIZE + BLOCK_GAP)
                start_y = block_row * (BLOCK_SIZE + BLOCK_GAP)
                
                for dy in range(BLOCK_SIZE):
                    for dx in range(BLOCK_SIZE):
                        px = start_x + dx
                        py = start_y + dy
                        if px < target_w and py < target_h:
                            bitmap[py][px] = 1
    
    return bitmap

def create_hash_bitmap():
    """Create '#' character bitmap - full grid (all 35 blocks lit) for background"""
    block_map = [[True] * BLOCK_COLS for _ in range(BLOCK_ROWS)]
    return block_map_to_bitmap(block_map)

def create_dot_bitmap():
    """Create '.' character bitmap - small dot"""
    block_map = [[False] * BLOCK_COLS for _ in range(BLOCK_ROWS)]
    
    # Single dot in center bottom
    block_map[5][2] = True
    
    return block_map_to_bitmap(block_map)

def create_space_bitmap():
    """Create space character bitmap - all empty"""
    block_map = [[False] * BLOCK_COLS for _ in range(BLOCK_ROWS)]
    return block_map_to_bitmap(block_map)

def bitmap_to_lvgl_data(bitmap):
    """Convert bitmap to LVGL 1bpp continuous bit-pack format"""
    h = len(bitmap)
    w = len(bitmap[0]) if h > 0 else 0
    
    data = []
    byte_val = 0
    bit_pos = 0
    
    for row in range(h):
        for col in range(w):
            if bitmap[row][col]:
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
    
    needed_ids = set([32, 35, 37, 45, 46, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58])
    
    selected_chars = {}
    for cid in sorted(needed_ids):
        if cid in chars:
            selected_chars[cid] = chars[cid]
    
    sorted_ids = sorted(selected_chars.keys())
    print(f"Selected {len(sorted_ids)} characters: {sorted_ids}")
    
    target_w = BLOCK_COLS * BLOCK_SIZE + (BLOCK_COLS - 1) * BLOCK_GAP
    target_h = BLOCK_ROWS * BLOCK_SIZE + (BLOCK_ROWS - 1) * BLOCK_GAP
    print(f"Target character size: {target_w}x{target_h} pixels (5x7 blocks, {BLOCK_SIZE}x{BLOCK_SIZE}px each, {BLOCK_GAP}px gap)")
    
    glyph_dscs = []
    bitmap_data = bytearray()
    
    for glyph_idx, cid in enumerate(sorted_ids):
        info = selected_chars[cid]
        w = target_w
        h = target_h
        
        if cid == 32:  # Space
            bitmap = create_space_bitmap()
            data = bitmap_to_lvgl_data(bitmap)
        elif cid == 35:  # '#' - create grid pattern
            bitmap = create_hash_bitmap()
            data = bitmap_to_lvgl_data(bitmap)
        elif cid == 46:  # '.' - create small dot (inverted for foreground)
            bitmap = create_dot_bitmap()
            bitmap = [[1 - bitmap[row][col] for col in range(len(bitmap[0]))] for row in range(len(bitmap))]
            data = bitmap_to_lvgl_data(bitmap)
        else:
            # Extract from PNG (already padded to 14x20)
            bitmap = extract_bitmap(img, info)
            # Invert: segments -> transparent(0), gaps -> foreground(1)
            bitmap = [[1 - bitmap[row][col] for col in range(len(bitmap[0]))] for row in range(len(bitmap))]
            data = bitmap_to_lvgl_data(bitmap)
        
        glyph_dsc = {
            'bitmap_index': len(bitmap_data),
            'adv_w': w * 10,
            'box_w': w,
            'box_h': h,
            'ofs_x': 0,
            'ofs_y': 0
        }
        glyph_dscs.append(glyph_dsc)
        bitmap_data.extend(data)
        print(f"  Char id={cid} (glyph {glyph_idx}): {w}x{h}, {len(data)} bytes, bitmap_index={glyph_dsc['bitmap_index']}")
    
    print(f"Total bitmap data size: {len(bitmap_data)} bytes")
    
    # Generate cmap entries
    cmap_entries = []
    for i, cid in enumerate(sorted_ids):
        cmap_entries.append({
            'range_start': cid,
            'range_length': 1,
            'glyph_id_start': i
        })
    
    with open(output_path, 'w') as f:
        f.write('/*\n')
        f.write(' * Auto-generated LVGL 9.x LED font\n')
        f.write(' * Source: Segment34.CN/resources/fonts/led.fnt + led.png\n')
        f.write(' * This font uses 1bpp (1 bit per pixel) format\n')
        f.write(' * Each character is 5x7 LED blocks (14x20 pixels)\n')
        f.write(' */\n\n')
        
        f.write('#include "lvgl.h"\n\n')
        
        # Write bitmap data
        f.write('/*Store the image of the glyphs*/\n')
        f.write('static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {\n')
        for i in range(0, len(bitmap_data), 16):
            chunk = bitmap_data[i:i+16]
            line = '    ' + ', '.join(f'0x{b:02X}' for b in chunk)
            if i + 16 < len(bitmap_data):
                line += ','
            f.write(line + '\n')
        f.write('};\n\n')
        
        # Write glyph descriptors
        f.write('/*Describe the properties of all glyphs*/\n')
        f.write('static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {\n')
        for i, gd in enumerate(glyph_dscs):
            comma = ',' if i < len(glyph_dscs) - 1 else ''
            f.write(f'    {{.bitmap_index = {gd["bitmap_index"]}, .adv_w = {gd["adv_w"]}, .box_w = {gd["box_w"]}, .box_h = {gd["box_h"]}, .ofs_x = {gd["ofs_x"]}, .ofs_y = {gd["ofs_y"]}}}{comma}\n')
        f.write('};\n\n')
        
        # Write character mapping
        f.write('/*Collect the unicode lists and glyph_id offsets*/\n')
        f.write(f'static const lv_font_fmt_txt_cmap_t cmaps[] = {{\n')
        for i, entry in enumerate(cmap_entries):
            comma = ',' if i < len(cmap_entries) - 1 else ''
            f.write('    {\n')
            f.write(f'        .range_start = {entry["range_start"]}, .range_length = {entry["range_length"]}, .glyph_id_start = {entry["glyph_id_start"]},\n')
            f.write('        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY\n')
            f.write('    }' + comma + '\n')
        f.write('};\n\n')
        
        # Write font descriptor
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
        
        # Write public font descriptor
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
    
    print(f"Generated font file: {output_path}")
    print(f"cmap_num: {len(cmap_entries)}")

if __name__ == '__main__':
    main()
