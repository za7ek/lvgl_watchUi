#!/usr/bin/env python3
"""Visualize # character from LED font"""

# # character bitmap (id=35, glyph 1, offset=20, 14x20, 40 bytes)
# From generated lv_font_led.c
bitmap_data = [
    0x49, 0x24, 0x92, 0x48, 0x00, 0x00, 0x49, 0x24, 0x92, 0x48, 0x00, 0x00,
    0x49, 0x24, 0x92, 0x48, 0x00, 0x00, 0x49, 0x24, 0x92, 0x48, 0x00, 0x00,
    0x49, 0x24, 0x92, 0x48, 0x00, 0x00, 0x49, 0x24, 0x92, 0x48, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
]

# 14 pixels wide, 20 rows
w = 14
h = 20

print("# character bitmap (14x20):")
byte_idx = 0
for row in range(h):
    row_bits = []
    for col in range(w):
        bit_in_byte = col % 8
        if bit_in_byte == 0 and col > 0:
            byte_idx += 1
        byte_val = bitmap_data[byte_idx]
        bit = (byte_val >> (7 - bit_in_byte)) & 1
        row_bits.append(bit)
    
    row_str = ''.join('█' if bit else '·' for bit in row_bits)
    print(f"  Row {row:2d}: {row_str}")
    
    if (col + 1) % 8 == 0 and col < w - 1:
        byte_idx += 1

# Let me redo this more carefully
print("\n# character bitmap (careful parsing):")
byte_idx = 0
for row in range(h):
    row_bits = []
    for col in range(w):
        bit_in_byte = col % 8
        if bit_in_byte == 0:
            if col > 0:
                byte_idx += 1
            if byte_idx >= len(bitmap_data):
                break
            current_byte = bitmap_data[byte_idx]
        bit = (current_byte >> (7 - bit_in_byte)) & 1
        row_bits.append(bit)
    
    row_str = ''.join('█' if bit else '·' for bit in row_bits)
    print(f"  Row {row:2d}: {row_str}")
    if byte_idx < len(bitmap_data) - 1:
        byte_idx += 1
