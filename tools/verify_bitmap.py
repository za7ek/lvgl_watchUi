#!/usr/bin/env python3
"""Verify bitmap packing with actual PNG data"""

from PIL import Image

def bitmap_to_lvgl_data(bitmap):
    """Convert bitmap to LVGL 1bpp format (MSB first)"""
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

def unpack_lvgl_data(data, w, h):
    """Unpack LVGL 1bpp data back to bitmap"""
    bitmap = []
    byte_idx = 0
    
    for row in range(h):
        row_bits = []
        for col in range(w):
            # Calculate which byte and which bit within that byte
            byte_in_row = col // 8
            bit_in_byte = 7 - (col % 8)
            
            if col == 0:
                # Start of new row
                if row > 0:
                    byte_idx += (w + 7) // 8
            elif col == 8:
                # Start of second byte in row
                byte_idx += 1
            
            if byte_idx < len(data):
                current_byte = data[byte_idx]
                bit = (current_byte >> bit_in_byte) & 1
            else:
                bit = 0
            
            row_bits.append(bit)
        
        bitmap.append(row_bits)
    
    return bitmap

# Load actual PNG data
png_path = '/home/zheng_fang/zephyr-project/Segment34.CN/resources/fonts/led.png'
img = Image.open(png_path)

# Test with '0' character (id=48, x=0, y=0, w=14, h=20)
x, y, w, h = 0, 0, 14, 20
char_img = img.crop((x, y, x + w, y + h))
char_img = char_img.convert('L')
pixels = list(char_img.getdata())

# Create bitmap from pixels
original_bitmap = []
for row in range(h):
    row_bits = []
    for col in range(w):
        idx = row * w + col
        pixel = pixels[idx]
        bit = 1 if pixel > 128 else 0
        row_bits.append(bit)
    original_bitmap.append(row_bits)

print("Original bitmap from PNG (14x20):")
for row in range(h):
    row_str = ''.join('█' if bit else '·' for bit in original_bitmap[row])
    print(f"  Row {row:2d}: {row_str}")

# Pack it
packed = bitmap_to_lvgl_data(original_bitmap)
print(f"\nPacked data ({len(packed)} bytes):")
for i in range(0, min(len(packed), 40), 16):
    chunk = packed[i:i+16]
    hex_str = ', '.join(f'0x{b:02X}' for b in chunk)
    print(f"  {hex_str}")

# Unpack and verify
unpacked = unpack_lvgl_data(packed, w, h)
print("\nUnpacked bitmap (verification):")
for row in range(h):
    row_str = ''.join('█' if bit else '·' for bit in unpacked[row])
    print(f"  Row {row:2d}: {row_str}")

# Compare
print("\nComparison:")
match = True
for row in range(h):
    orig_str = ''.join('█' if bit else '·' for bit in original_bitmap[row])
    unpack_str = ''.join('█' if bit else '·' for bit in unpacked[row])
    if orig_str != unpack_str:
        print(f"  MISMATCH Row {row}:")
        print(f"    Original: {orig_str}")
        print(f"    Unpacked: {unpack_str}")
        match = False

if match:
    print("  ✅ All rows match!")
else:
    print("  ❌ Mismatch found!")

# Now check what LVGL expects
# Let's look at the actual generated font data and see if it can be correctly unpacked
print("\n\nChecking actual generated font data for '0' character...")
# From lv_font_led.c, '0' is glyph 5 (id=48), bitmap_index=160
# Let me extract the first 40 bytes (for 14x20 character)
actual_font_data = bytes([
    0x1B, 0x60, 0x1B, 0x60, 0x00, 0x00, 0xC0, 0x0C, 0xC0, 0x0C, 0x00, 0x00,
    0xC0, 0x6C, 0xC0, 0x6C, 0x00, 0x00, 0xC3, 0x0C, 0xC3, 0x0C, 0x00, 0x00,
    0xD8, 0x0C, 0xD8, 0x0C, 0x00, 0x00, 0xC0, 0x0C, 0xC0, 0x0C, 0x00, 0x00,
    0x1B, 0x60, 0x1B, 0x60
])

print(f"Actual font data for '0' ({len(actual_font_data)} bytes):")
unpacked_font = unpack_lvgl_data(actual_font_data, w, h)
for row in range(h):
    row_str = ''.join('█' if bit else '·' for bit in unpacked_font[row])
    print(f"  Row {row:2d}: {row_str}")

# Compare with original
print("\nFont data vs original:")
for row in range(h):
    orig_str = ''.join('█' if bit else '·' for bit in original_bitmap[row])
    font_str = ''.join('█' if bit else '·' for bit in unpacked_font[row])
    if orig_str != font_str:
        print(f"  MISMATCH Row {row}:")
        print(f"    Original: {orig_str}")
        print(f"    Font data: {font_str}")
