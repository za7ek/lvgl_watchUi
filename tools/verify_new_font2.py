#!/usr/bin/env python3
"""Verify the newly generated LED font - simplified version"""

# Read the generated font file
with open('/home/zheng_fang/zephyr-project/lgvl_watchUi/app/src/fonts/lv_font_led.c', 'r') as f:
    content = f.read()

# Parse glyph descriptors using simpler method
glyph_lines = []
for line in content.split('\n'):
    if '.bitmap_index' in line and '.box_w' in line:
        # Extract values
        bi = int(line.split('.bitmap_index = ')[1].split(',')[0])
        aw = int(line.split('.adv_w = ')[1].split(',')[0])
        bw = int(line.split('.box_w = ')[1].split(',')[0])
        bh = int(line.split('.box_h = ')[1].split(',')[0])
        ox = int(line.split('.ofs_x = ')[1].split(',')[0])
        oy = int(line.split('.ofs_y = ')[1].split('}')[0])
        glyph_lines.append({
            'bitmap_index': bi,
            'adv_w': aw,
            'box_w': bw,
            'box_h': bh,
            'ofs_x': ox,
            'ofs_y': oy
        })

print(f"Glyph count: {len(glyph_lines)}")

# Parse bitmap data
import re

bitmap_match = re.search(r'glyph_bitmap\[\] = \{(.*?)\};', content, re.DOTALL)
if bitmap_match:
    bitmap_hex = bitmap_match.group(1)
    bitmap_data = []
    for x in bitmap_hex.split(','):
        x = x.strip()
        if x:
            try:
                bitmap_data.append(int(x, 16))
            except:
                pass
    print(f"Bitmap data size: {len(bitmap_data)} bytes")

# Character mapping
char_ids = [32, 35, 37, 45, 46, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58]
char_names = {
    32: 'Space', 35: '#', 37: '%', 45: '-', 46: '.',
    48: '0', 49: '1', 50: '2', 51: '3', 52: '4',
    53: '5', 54: '6', 55: '7', 56: '8', 57: '9', 58: ':'
}

def decode_font_continuous(data, bitmap_index, box_w, box_h):
    """Decode with continuous bit-packing (LVGL format)"""
    bitmap = []
    
    # Convert data to bit stream
    bits = []
    total_bits = box_w * box_h
    total_bytes = (total_bits + 7) // 8
    
    for i in range(bitmap_index, bitmap_index + total_bytes):
        if i < len(data):
            for bit in range(7, -1, -1):  # MSB first
                bits.append((data[i] >> bit) & 1)
    
    bit_pos = 0
    for row in range(box_h):
        row_bits = []
        for col in range(box_w):
            if bit_pos < len(bits):
                row_bits.append(bits[bit_pos])
                bit_pos += 1
            else:
                row_bits.append(0)
        bitmap.append(row_bits)
    
    return bitmap

print("\nVerifying character bitmaps:")
print("=" * 60)

for i, glyph in enumerate(glyph_lines):
    char_id = char_ids[i] if i < len(char_ids) else 0
    char_name = char_names.get(char_id, f'id={char_id}')
    
    print(f"\nCharacter {i}: '{char_name}' (id={char_id}), size={glyph['box_w']}x{glyph['box_h']}")
    
    bitmap = decode_font_continuous(bitmap_data, glyph['bitmap_index'], glyph['box_w'], glyph['box_h'])
    
    # Print first 5 rows
    for row in range(min(5, len(bitmap))):
        row_str = ''.join('█' if bit else '·' for bit in bitmap[row])
        print(f"  Row {row:2d}: {row_str}")
    
    if glyph['box_h'] > 5:
        print(f"  ... ({glyph['box_h'] - 5} more rows)")

# Verify total bytes calculation
print("\n\nByte verification:")
total_calculated = 0
for glyph in glyph_lines:
    bytes_needed = (glyph['box_w'] * glyph['box_h'] + 7) // 8
    total_calculated += bytes_needed
print(f"Total calculated bytes: {total_calculated}")
print(f"Total actual bytes: {len(bitmap_data)}")

if total_calculated == len(bitmap_data):
    print("✓ Byte count matches!")
else:
    print("✗ Byte count MISMATCH!")
    print(f"  Difference: {len(bitmap_data) - total_calculated} bytes")

# Verify bitmap_index values are correct
print("\nBitmap index verification:")
expected_index = 0
for i, glyph in enumerate(glyph_lines):
    if glyph['bitmap_index'] != expected_index:
        print(f"  ✗ Glyph {i} has bitmap_index={glyph['bitmap_index']}, expected={expected_index}")
    expected_index += (glyph['box_w'] * glyph['box_h'] + 7) // 8
print("  Done!")
