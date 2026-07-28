#!/usr/bin/env python3
"""Verify generated LVGL font by comparing bitmap data with original PNG."""

import sys
import re
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
                    elif k == 'xoffset': cdata['xoff'] = int(v)
                    elif k == 'yoffset': cdata['yoff'] = int(v)
                    elif k == 'xadvance': cdata['xadv'] = int(v)
                chars[cid] = cdata
    return common, chars

def detect_dark_background(img):
    """Auto-detect if the PNG has a dark background (white text on black)."""
    gray = img.convert('L')
    w, h = gray.size
    samples = [gray.getpixel((0, 0))]
    if w > 1: samples.append(gray.getpixel((w-1, 0)))
    if h > 1: samples.append(gray.getpixel((0, h-1)))
    if w > 1 and h > 1: samples.append(gray.getpixel((w-1, h-1)))
    avg = sum(samples) / len(samples)
    return avg < 128


def extract_bitmap_from_png(img, x, y, w, h, dark_bg=False):
    """Extract bitmap from PNG, return 2D list (0=foreground, 1=background)."""
    img_gray = img.convert('L')
    raw = img_gray.crop((x, y, x + w, y + h))
    raw_pixels = list(raw.getdata())
    
    bitmap = [[1] * w for _ in range(h)]
    for row in range(h):
        for col in range(w):
            pixel_val = raw_pixels[row * w + col]
            if dark_bg:
                is_fg = pixel_val > 128
            else:
                is_fg = pixel_val < 128
            bitmap[row][col] = 0 if is_fg else 1
    return bitmap

def bitmap_to_lvgl_1bpp(bitmap, width, height):
    """Convert bitmap to LVGL 1bpp MSB-first format."""
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

def parse_c_file_bitmap(c_path):
    """Parse the glyph_bitmap array from a C font file."""
    with open(c_path, 'r') as f:
        content = f.read()
    
    # Find the glyph_bitmap array
    match = re.search(r'static\s+LV_ATTRIBUTE_LARGE_CONST\s+const\s+uint8_t\s+glyph_bitmap\[\]\s*=\s*\{([^}]+)\}', content)
    if not match:
        print("ERROR: Could not find glyph_bitmap array")
        return None
    
    data_str = match.group(1)
    # Extract all hex values
    hex_vals = re.findall(r'0x([0-9A-Fa-f]+)', data_str)
    bitmap_data = bytes([int(h, 16) for h in hex_vals])
    return bitmap_data

def parse_c_file_glyph_dsc(c_path):
    """Parse glyph descriptors from a C font file."""
    with open(c_path, 'r') as f:
        content = f.read()
    
    # Find the glyph_dsc array
    match = re.search(r'static\s+const\s+lv_font_fmt_txt_glyph_dsc_t\s+glyph_dsc\[\]\s*=\s*\{(.+?)\};', content, re.DOTALL)
    if not match:
        print("ERROR: Could not find glyph_dsc array")
        return None
    
    dsc_str = match.group(1)
    entries = []
    for line in dsc_str.split('\n'):
        line = line.strip()
        if not line or line.startswith('/*') or not line.startswith('{'):
            continue
        # Extract values from { ... }
        vals = re.findall(r'[\d]+', line)
        if len(vals) >= 6:
            unicode_match = re.search(r'/\*\s*0x([0-9A-Fa-f]+)\s*\*/', line)
            unicode_val = int(unicode_match.group(1), 16) if unicode_match else int(vals[6])
            entries.append({
                'bitmap_index': int(vals[0]),
                'adv_w': int(vals[1]),
                'box_w': int(vals[2]),
                'box_h': int(vals[3]),
                'ofs_x': int(vals[4]),
                'ofs_y': int(vals[5]),
                'unicode': unicode_val
            })
    return entries

def verify_font(fnt_path, png_path, c_path):
    """Verify a generated font by comparing PNG extraction with C file data."""
    print(f"Verifying font: {c_path}")
    print(f"  FNT: {fnt_path}")
    print(f"  PNG: {png_path}")
    print()
    
    # Parse FNT
    common, chars = parse_fnt(fnt_path)
    print(f"FNT: {len(chars)} characters, lineHeight={common.get('lineHeight')}, base={common.get('base')}")
    
    # Load PNG
    img = Image.open(png_path)
    print(f"PNG: {img.size}")
    
    # Auto-detect background polarity
    dark_bg = detect_dark_background(img)
    print(f"Background polarity: {'dark bg / light text' if dark_bg else 'light bg / dark text'}")
    
    # Parse C file
    c_bitmap = parse_c_file_bitmap(c_path)
    c_dsc = parse_c_file_glyph_dsc(c_path)
    
    if c_bitmap is None or c_dsc is None:
        print("ERROR: Failed to parse C file")
        return False
    
    print(f"C file: {len(c_dsc)} glyphs, {len(c_bitmap)} bytes bitmap")
    
    # Check 1: Number of glyphs matches
    if len(c_dsc) != len(chars):
        print(f"WARNING: Glyph count mismatch: FNT has {len(chars)}, C file has {len(c_dsc)}")
    
    # Check 2: Compare each glyph
    errors = []
    for i, dsc in enumerate(c_dsc):
        cid = dsc['unicode']
        if cid not in chars:
            print(f"  WARNING: Char {cid} (0x{cid:04X}) not found in FNT")
            continue
        
        info = chars[cid]
        box_w = info['w']
        box_h = info['h']
        
        # Extract bitmap from PNG
        png_bitmap = extract_bitmap_from_png(img, info['x'], info['y'], box_w, box_h, dark_bg)
        png_1bpp = bitmap_to_lvgl_1bpp(png_bitmap, box_w, box_h)
        
        # Get bitmap from C file
        bitmap_idx = dsc['bitmap_index']
        c_1bpp = c_bitmap[bitmap_idx:bitmap_idx + len(png_1bpp)]
        
        # Compare
        if png_1bpp != c_1bpp:
            errors.append({
                'cid': cid,
                'name': chr(cid) if 32 <= cid <= 126 else f'0x{cid:04X}',
                'png': png_1bpp.hex(),
                'c': c_1bpp.hex(),
                'box_w': box_w,
                'box_h': box_h
            })
        
        # Check metrics
        expected_adv_w = info['xadv'] * 16
        if dsc['adv_w'] != expected_adv_w:
            errors.append({
                'cid': cid,
                'name': chr(cid) if 32 <= cid <= 126 else f'0x{cid:04X}',
                'metric': f"adv_w mismatch: expected {expected_adv_w}, got {dsc['adv_w']}"
            })
        
        expected_ofs_y = common.get('lineHeight', 0) - box_h - info['yoff']
        if dsc['ofs_y'] != expected_ofs_y:
            errors.append({
                'cid': cid,
                'name': chr(cid) if 32 <= cid <= 126 else f'0x{cid:04X}',
                'metric': f"ofs_y mismatch: expected {expected_ofs_y}, got {dsc['ofs_y']} (yoff={info['yoff']}, box_h={box_h}, lineHeight={common.get('lineHeight')})"
            })
    
    if errors:
        print(f"\nERRORS FOUND: {len(errors)}")
        for err in errors[:10]:  # Show first 10 errors
            if 'metric' in err:
                print(f"  Char '{err['name']}' (id={err['cid']}): {err['metric']}")
            else:
                print(f"  Char '{err['name']}' (id={err['cid']}) bitmap mismatch:")
                print(f"    Expected (from PNG): {err['png']}")
                print(f"    Got (from C file):  {err['c']}")
        if len(errors) > 10:
            print(f"  ... and {len(errors) - 10} more errors")
        return False
    else:
        print("\nSUCCESS: All glyphs match!")
        return True

if __name__ == '__main__':
    base = '/home/zheng_fang/zephyr-project'
    
    fonts = [
        ('xsmol', f'{base}/Segment34.CN/resources/fonts/xsmol.fnt', 
                f'{base}/Segment34.CN/resources/fonts/xsmol.png',
                f'{base}/lgvl_watchUi/app/src/fonts/lv_font_xsmol.c'),
        ('led_small', f'{base}/Segment34.CN/resources/fonts/led_small.fnt',
                       f'{base}/Segment34.CN/resources/fonts/led_small.png',
                       f'{base}/lgvl_watchUi/app/src/fonts/lv_font_led_small.c'),
    ]
    
    all_ok = True
    for name, fnt, png, c in fonts:
        print("=" * 60)
        if not verify_font(fnt, png, c):
            all_ok = False
        print()
    
    if all_ok:
        print("=" * 60)
        print("ALL FONTS VERIFIED SUCCESSFULLY!")
    else:
        print("=" * 60)
        print("SOME FONTS HAVE ERRORS!")
