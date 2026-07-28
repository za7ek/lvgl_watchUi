#!/usr/bin/env python3
"""Compare LVGL 1bpp font formats"""

# LVGL reference font data for "#" character (id=35)
# From test_font_montserrat_ascii_1bpp.c
ref_hash_bitmap = bytes([
    0x8, 0x40, 0x42, 0x2, 0x10, 0xff, 0xf0, 0x8c,
    0xc, 0x60, 0x63, 0x3, 0x18, 0x18, 0x87, 0xff,
    0x84, 0x20, 0x21, 0x1, 0x8, 0x8, 0x40
])

# Also check the space character (id=32)
ref_space_bitmap = bytes([0x0])  # Just one byte for space

# Let's try to decode these using MSB-first convention
def decode_bitmap_msb(data, w, h):
    """Decode using MSB-first convention"""
    bitmap = []
    byte_idx = 0
    
    for row in range(h):
        row_bits = []
        for col in range(w):
            byte_in_row = col // 8
            bit_in_byte = 7 - (col % 8)
            
            if col == 0 and row > 0:
                byte_idx += (w + 7) // 8
            
            if byte_idx < len(data):
                bit = (data[byte_idx + byte_in_row] >> bit_in_byte) & 1
            else:
                bit = 0
            
            row_bits.append(bit)
        
        bitmap.append(row_bits)
    
    return bitmap

def decode_bitmap_lsb(data, w, h):
    """Decode using LSB-first convention"""
    bitmap = []
    byte_idx = 0
    
    for row in range(h):
        row_bits = []
        for col in range(w):
            byte_in_row = col // 8
            bit_in_byte = col % 8
            
            if col == 0 and row > 0:
                byte_idx += (w + 7) // 8
            
            if byte_idx < len(data):
                bit = (data[byte_idx + byte_in_row] >> bit_in_byte) & 1
            else:
                bit = 0
            
            row_bits.append(bit)
        
        bitmap.append(row_bits)
    
    return bitmap

# The reference font "#" is 12 pixels wide (based on typical montserrat fonts)
# Let's try different widths
print("Decoding LVGL reference font '#' character:")
for w in [8, 10, 12, 14, 16]:
    h = len(ref_hash_bitmap) * 8 // w  # Approximate height
    if h < 1:
        continue
    
    # Adjust h to reasonable value
    if w == 12:
        h = 14
    elif w == 8:
        h = 20
    else:
        h = min(20, len(ref_hash_bitmap) * 8 // w + 1)
    
    print(f"\n  Trying {w}x{h}...")
    
    # Try MSB first
    decoded_msb = decode_bitmap_msb(ref_hash_bitmap, w, h)
    print(f"  MSB-first:")
    for row in range(min(5, h)):
        row_str = ''.join('█' if bit else '·' for bit in decoded_msb[row])
        print(f"    Row {row:2d}: {row_str}")
    
    # Try LSB first
    decoded_lsb = decode_bitmap_lsb(ref_hash_bitmap, w, h)
    print(f"  LSB-first:")
    for row in range(min(5, h)):
        row_str = ''.join('█' if bit else '·' for bit in decoded_lsb[row])
        print(f"    Row {row:2d}: {row_str}")

# Now let's check our font format
print("\n\nOur font '#' character (14x20):")
our_hash_bitmap = bytes([
    0x49, 0x24, 0x92, 0x48, 0x00, 0x00, 0x49, 0x24, 0x92, 0x48, 0x00, 0x00,
    0x49, 0x24, 0x92, 0x48, 0x00, 0x00, 0x49, 0x24, 0x92, 0x48, 0x00, 0x00,
    0x49, 0x24, 0x92, 0x48, 0x00, 0x00, 0x49, 0x24, 0x92, 0x48, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
])

w, h = 14, 20
print(f"  MSB-first:")
decoded_msb = decode_bitmap_msb(our_hash_bitmap, w, h)
for row in range(min(5, h)):
    row_str = ''.join('█' if bit else '·' for bit in decoded_msb[row])
    print(f"    Row {row:2d}: {row_str}")

print(f"  LSB-first:")
decoded_lsb = decode_bitmap_lsb(our_hash_bitmap, w, h)
for row in range(min(5, h)):
    row_str = ''.join('█' if bit else '·' for bit in decoded_lsb[row])
    print(f"    Row {row:2d}: {row_str}")

# Expected pattern from PNG:
print("\nExpected pattern from PNG:")
print("  Row  0: ██·██·██·██·██")
print("  Row  1: ██·██·██·██·██")
print("  Row  2: ··············")
print("  Row  3: ██·██·██·██·██")
print("  Row  4: ██·██·██·██·██")
