#!/usr/bin/env python3
"""Analyze LVGL 1bpp font format"""

# Reference font bitmap data
bitmap_data = [
    0x0,  # space

    0xff, 0xff, 0xf0, 0x30,  # !

    0xde, 0xf7, 0xbd, 0x80,  # "

    0x8, 0x40, 0x42, 0x2, 0x10, 0xff, 0xf0, 0x8c, 0xc, 0x60, 0x63, 0x3, 0x18, 0x18, 0x87, 0xff, 0x84, 0x20, 0x21, 0x1, 0x8, 0x8, 0x40,  # #

    0xc, 0x3, 0x0, 0xc0, 0xfe, 0x6c, 0xb3, 0xc, 0xc3, 0x30, 0xec, 0x1f, 0x81, 0xf8, 0x37, 0xc, 0xc3, 0x38, 0xcf, 0x36, 0x7f, 0x3, 0x0, 0xc0, 0x30,  # $
]

# Glyph info from glyph_dsc
glyphs = [
    # id, bitmap_index, adv_w, box_w, box_h, ofs_x, ofs_y
    (0, 0, 0, 0, 0, 0, 0),  # reserved
    (32, 0, 86, 1, 1, 0, 0),  # space
    (33, 1, 86, 2, 14, 1, 0),  # !
    (34, 5, 125, 5, 5, 1, 0),  # "
    (35, 9, 225, 13, 14, 1, 0),  # #
]

def decode_font(data, bitmap_index, box_w, box_h):
    """Try to decode LVGL 1bpp font data"""
    bitmap = []
    idx = bitmap_index
    
    for row in range(box_h):
        row_bits = []
        for col in range(box_w):
            # Calculate bit position within the row
            bit_idx = row * box_w + col
            
            # Calculate which byte and which bit
            byte_offset = bit_idx // 8
            bit_offset = 7 - (bit_idx % 8)  # MSB first
            
            if idx + byte_offset < len(data):
                bit = (data[idx + byte_offset] >> bit_offset) & 1
            else:
                bit = 0
            
            row_bits.append(bit)
        
        bitmap.append(row_bits)
    
    return bitmap

print("Analyzing LVGL 1bpp format:")
print("=" * 60)

# Decode space character (1x1)
print("\nSpace (1x1):")
bitmap = decode_font(bitmap_data, 0, 1, 1)
for row in range(min(3, len(bitmap))):
    row_str = ''.join('█' if bit else '·' for bit in bitmap[row])
    print(f"  Row {row:2d}: {row_str}")
    print(f"  Expected: 0x0 = empty")

# Decode ! character (2x14)
print("\n'!' (2x14):")
bitmap = decode_font(bitmap_data, 1, 2, 14)
for row in range(min(14, len(bitmap))):
    row_str = ''.join('█' if bit else '·' for bit in bitmap[row])
    print(f"  Row {row:2d}: {row_str}")

# Decode " character (5x5)
print("\n'\"' (5x5):")
bitmap = decode_font(bitmap_data, 5, 5, 5)
for row in range(min(5, len(bitmap))):
    row_str = ''.join('█' if bit else '·' for bit in bitmap[row])
    print(f"  Row {row:2d}: {row_str}")

# Decode # character (13x14)
print("\n'#' (13x14):")
bitmap = decode_font(bitmap_data, 9, 13, 14)
for row in range(min(14, len(bitmap))):
    row_str = ''.join('█' if bit else '·' for bit in bitmap[row])
    print(f"  Row {row:2d}: {row_str}")

# Now check the actual bytes used
print("\n\nByte analysis:")
for glyph_id, bmp_idx, adv_w, box_w, box_h, ofs_x, ofs_y in glyphs[1:]:
    total_bits = box_w * box_h
    total_bytes_needed = (total_bits + 7) // 8
    print(f"  id={glyph_id}, size={box_w}x{box_h}, bits={total_bits}, bytes_needed={total_bytes_needed}")

# Let's also check with LSB-first
print("\n\nTrying LSB-first for '#' character:")
def decode_font_lsb(data, bitmap_index, box_w, box_h):
    """Try to decode LVGL 1bpp font data with LSB first"""
    bitmap = []
    idx = bitmap_index
    
    for row in range(box_h):
        row_bits = []
        for col in range(box_w):
            bit_idx = row * box_w + col
            byte_offset = bit_idx // 8
            bit_offset = bit_idx % 8  # LSB first
            
            if idx + byte_offset < len(data):
                bit = (data[idx + byte_offset] >> bit_offset) & 1
            else:
                bit = 0
            
            row_bits.append(bit)
        
        bitmap.append(row_bits)
    
    return bitmap

bitmap = decode_font_lsb(bitmap_data, 9, 13, 14)
for row in range(min(14, len(bitmap))):
    row_str = ''.join('█' if bit else '·' for bit in bitmap[row])
    print(f"  Row {row:2d}: {row_str}")

# Now try the CORRECT approach - bit-packed without byte alignment per row
print("\n\nTrying continuous bit-packing (no row alignment):")
def decode_font_continuous(data, bitmap_index, box_w, box_h):
    """Decode with continuous bit-packing"""
    bitmap = []
    
    # Convert data to bit stream
    bits = []
    for i in range(bitmap_index, len(data)):
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

# Test space
print("\nSpace (1x1):")
bitmap = decode_font_continuous(bitmap_data, 0, 1, 1)
for row in bitmap:
    row_str = ''.join('█' if bit else '·' for bit in row)
    print(f"  {row_str}")

# Test !
print("\n'!' (2x14):")
bitmap = decode_font_continuous(bitmap_data, 1, 2, 14)
for row in range(min(14, len(bitmap))):
    row_str = ''.join('█' if bit else '·' for bit in bitmap[row])
    print(f"  Row {row:2d}: {row_str}")

# Test "
print("\n'\"' (5x5):")
bitmap = decode_font_continuous(bitmap_data, 5, 5, 5)
for row in range(min(5, len(bitmap))):
    row_str = ''.join('█' if bit else '·' for bit in bitmap[row])
    print(f"  Row {row:2d}: {row_str}")

# Test #
print("\n'#' (13x14):")
bitmap = decode_font_continuous(bitmap_data, 9, 13, 14)
for row in range(min(14, len(bitmap))):
    row_str = ''.join('█' if bit else '·' for bit in bitmap[row])
    print(f"  Row {row:2d}: {row_str}")

# Calculate total bytes used
print("\n\nTotal bytes calculation:")
total_bits_used = 0
for glyph_id, bmp_idx, adv_w, box_w, box_h, ofs_x, ofs_y in glyphs[1:]:
    bits = box_w * box_h
    bytes_needed = (bits + 7) // 8
    total_bits_used += bytes_needed
    print(f"  id={glyph_id}: {bits} bits = {bytes_needed} bytes")
print(f"  Total bytes (if no row alignment): {total_bits_used}")

# Check actual bytes from index 9 to end of '#' data
# We need to know where '#' ends
# From the data, '#' is followed by '$' which starts at some index
# Let's count the actual bytes for '#'
print(f"\n  Actual bytes for '#' (from index 9): 23 bytes")
print(f"  Calculated bytes (continuous): {(13*14 + 7) // 8} = {(13*14 + 7) // 8}")
