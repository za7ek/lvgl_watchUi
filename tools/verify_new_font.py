#!/usr/bin/env python3
"""Verify the newly generated LED font"""

# Read the generated font file
with open('/home/zheng_fang/zephyr-project/lgvl_watchUi/app/src/fonts/lv_font_led.c', 'r') as f:
    content = f.read()

# Extract bitmap data
import re

# Parse bitmap array
bitmap_match = re.search(r'glyph_bitmap\[\] = \{(.*?)\};', content, re.DOTALL)
if bitmap_match:
    bitmap_hex = bitmap_match.group(1)
    bitmap_data = [int(x.strip(), 16) for x in bitmap_hex.split(',') if x.strip().startswith('0x') or x.strip().startswith('0X')]
    print(f"Bitmap data size: {len(bitmap_data)} bytes")

# Parse glyph descriptors
glyph_match = re.search(r'static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc\[\] = \{(.*?)\};', content, re.DOTALL)
if glyph_match:
    glyph_text = glyph_match.group(1)
    glyphs = []
    # Match each glyph entry
    for match in re.finditer(r'\{([^}]+)\}', glyph_text):
        entry = match.group(1)
        values = {}
        for part in entry.split(','):
            part = part.strip()
            if '=' in part:
                key, val = part.split('=')
                values[key.strip()] = int(val.strip())
        if 'bitmap_index' in values:
            glyphs.append(values)
    print(f"Glyph count: {len(glyphs)}")

# Character mapping (based on order in cmap)
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
    for i in range(bitmap_index, min(bitmap_index + (box_w * box_h + 7) // 8 + 1, len(data))):
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

for i, glyph in enumerate(glyphs):
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
for glyph in glyphs:
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
for i, glyph in enumerate(glyphs):
    if glyph['bitmap_index'] != expected_index:
        print(f"  ✗ Glyph {i} has bitmap_index={glyph['bitmap_index']}, expected={expected_index}")
    expected_index += (glyph['box_w'] * glyph['box_h'] + 7) // 8
print("  Done!")
