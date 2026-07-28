#!/usr/bin/env python3
"""Test LED font bitmap generation"""

import sys
from PIL import Image

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
    x, y, w, h = char_info['x'], char_info['y'], char_info['w'], char_info['h']
    
    char_img = img.crop((x, y, x + w, y + h))
    char_img = char_img.convert('L')
    pixels = list(char_img.getdata())
    
    width = char_info['w']
    height = char_info['h']
    
    bitmap = []
    for row in range(height):
        row_bits = []
        for col in range(width):
            idx = row * width + col
            pixel = pixels[idx]
            bit = 1 if pixel > 128 else 0
            row_bits.append(bit)
        bitmap.append(row_bits)
    
    return bitmap

def bitmap_to_lvgl_data(bitmap):
    h = len(bitmap)
    w = len(bitmap[0]) if h > 0 else 0
    
    data = []
    for row in range(h):
        byte_val = 0
        bit_pos = 0
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
    
    chars = parse_fnt(fnt_path)
    print(f"Parsed {len(chars)} characters from FNT")
    
    img = Image.open(png_path)
    print(f"Image size: {img.size}")
    
    needed_ids = set([48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 45, 46, 37, 32, 58])
    
    selected_chars = {}
    for cid, info in chars.items():
        if cid in needed_ids:
            selected_chars[cid] = info
    
    sorted_ids = sorted(selected_chars.keys())
    print(f"\nSelected {len(sorted_ids)} characters:")
    
    total_expected = 0
    total_actual = 0
    offset = 0
    
    for cid in sorted_ids:
        info = selected_chars[cid]
        w = info['w']
        h = info['h']
        
        bitmap = extract_bitmap(img, info)
        data = bitmap_to_lvgl_data(bitmap)
        
        expected_bytes = ((w + 7) // 8) * h
        actual_bytes = len(data)
        
        char_name = chr(cid) if 32 <= cid <= 126 else f'\\u{cid}'
        
        print(f"  Char '{char_name}' (id={cid}): {w}x{h}, expected={expected_bytes} bytes, actual={actual_bytes} bytes")
        print(f"    bitmap_index: {offset}, data[:20]: {data[:20].hex()}")
        
        if actual_bytes != expected_bytes:
            print(f"    ❌ MISMATCH!")
        
        total_expected += expected_bytes
        total_actual += actual_bytes
        offset += actual_bytes
    
    print(f"\nTotal: expected={total_expected} bytes, actual={total_actual} bytes")
    
    # Check if the actual data matches expected
    if total_expected == total_actual:
        print("✅ Total size matches!")
    else:
        print("❌ Total size mismatch!")
    
    # Let's dump the first character's bitmap for visual inspection
    print("\nFirst character (space, id=32) bitmap:")
    info = selected_chars[32]
    bitmap = extract_bitmap(img, info)
    for row_idx, row in enumerate(bitmap):
        row_str = ''.join('█' if bit else '·' for bit in row)
        print(f"  Row {row_idx:2d}: {row_str}")
    
    # Check '0' character
    print("\nCharacter '0' (id=48) bitmap:")
    info = selected_chars[48]
    bitmap = extract_bitmap(img, info)
    for row_idx, row in enumerate(bitmap):
        row_str = ''.join('█' if bit else '·' for bit in row)
        print(f"  Row {row_idx:2d}: {row_str}")

if __name__ == '__main__':
    main()
