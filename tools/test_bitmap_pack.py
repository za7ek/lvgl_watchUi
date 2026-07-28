#!/usr/bin/env python3
"""Test bitmap packing for LVGL 1bpp font"""

# Simulate the bitmap_to_lvgl_data function
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

# Test with known bitmap pattern: "#" character pattern
# From the original PNG: "██·██·██·██·██" pattern
# Let's create a test bitmap that matches the expected "#" pattern

# Expected "#" character bitmap (14x20) based on original PNG:
# Pattern: "██·██·██·██·██" repeating every 2 rows, with gap every 3rd row
test_bitmap = []
for row in range(20):
    if row % 3 == 2:
        # Empty row
        test_bitmap.append([0] * 14)
    else:
        # Pattern row: "██·██·██·██·██"
        row_bits = []
        for i in range(14):
            # Every 3rd position is 0, others are 1
            if i % 3 == 2:
                row_bits.append(0)
            else:
                row_bits.append(1)
        test_bitmap.append(row_bits)

print("Test bitmap (14x20):")
for row in range(20):
    row_str = ''.join('█' if bit else '·' for bit in test_bitmap[row])
    print(f"  Row {row:2d}: {row_str}")

# Now pack it
packed_data = bitmap_to_lvgl_data(test_bitmap)
print(f"\nPacked data ({len(packed_data)} bytes):")
for i in range(0, len(packed_data), 16):
    chunk = packed_data[i:i+16]
    hex_str = ', '.join(f'0x{b:02X}' for b in chunk)
    print(f"  {hex_str}")

# Now let's try to unpack it and verify
print("\nUnpacked verification:")
byte_idx = 0
for row in range(20):
    row_bits = []
    for col in range(14):
        byte_in_row = col // 8
        bit_in_byte = 7 - (col % 8)
        
        if byte_in_row == 0:
            if col > 0 and col % 8 == 0:
                byte_idx += 1
            current_byte = packed_data[byte_idx]
        
        bit = (current_byte >> (7 - (col % 8))) & 1
        row_bits.append(bit)
    
    row_str = ''.join('█' if bit else '·' for bit in row_bits)
    print(f"  Row {row:2d}: {row_str}")
    
    # Move to next row's bytes
    byte_idx += (14 + 7) // 8  # 2 bytes per row

# Let me also check the actual generated data from the font
print("\n\nComparing with actual generated font data...")
# The "#" character in the generated font starts at offset 20
# Let's extract it and visualize

actual_packed = bytes([
    0x49, 0x24, 0x92, 0x48, 0x00, 0x00, 0x49, 0x24, 0x92, 0x48, 0x00, 0x00,
    0x49, 0x24, 0x92, 0x48, 0x00, 0x00, 0x49, 0x24, 0x92, 0x48, 0x00, 0x00,
    0x49, 0x24, 0x92, 0x48, 0x00, 0x00, 0x49, 0x24, 0x92, 0x48, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
])

print(f"\nActual packed data ({len(actual_packed)} bytes):")
byte_idx = 0
for row in range(20):
    row_bits = []
    for col in range(14):
        byte_in_row = col // 8
        bit_in_byte = 7 - (col % 8)
        
        if col == 0 and row > 0:
            byte_idx += (14 + 7) // 8
        elif col == 8:
            byte_idx += 1
        
        if byte_idx < len(actual_packed):
            current_byte = actual_packed[byte_idx]
            bit = (current_byte >> (7 - (col % 8))) & 1
            row_bits.append(bit)
        else:
            row_bits.append(0)
    
    row_str = ''.join('█' if bit else '·' for bit in row_bits)
    print(f"  Row {row:2d}: {row_str}")
