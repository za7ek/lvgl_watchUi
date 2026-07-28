#!/usr/bin/env python3
"""Analyze LED font PNG structure - understand 5x7 block layout"""

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

fnt_path = '/home/zheng_fang/zephyr-project/Segment34.CN/resources/fonts/led.fnt'
png_path = '/home/zheng_fang/zephyr-project/Segment34.CN/resources/fonts/led.png'

chars = parse_fnt(fnt_path)
img = Image.open(png_path)

print(f"Image size: {img.size}")
print(f"Parsed {len(chars)} characters")
print("\nCharacter details:")

# Analyze '0' character structure
for cid in [48, 35]:  # '0' and '#'
    if cid in chars:
        info = chars[cid]
        print(f"\nCharacter id={cid} ({chr(cid) if cid < 128 else '?'}): {info['w']}x{info['h']}")
        
        # Extract and display the bitmap
        char_img = img.crop((info['x'], info['y'], info['x'] + info['w'], info['y'] + info['h']))
        char_img = char_img.convert('L')
        pixels = list(char_img.getdata())
        
        w, h = info['w'], info['h']
        print(f"  Pixel structure ({w}x{h}):")
        
        # Display as grid
        for row in range(h):
            row_str = "  Row "
            if row < 10:
                row_str += " "
            row_str += f"{row}: "
            for col in range(w):
                pixel = pixels[row * w + col]
                # 0 = black (lit), 255 = white (not lit)
                if pixel < 128:
                    row_str += "██"  # Lit pixel
                else:
                    row_str += "··"  # Not lit
            print(row_str)

print("\n\nAnalyzing 5x7 block structure:")
print("If character is 14x20 pixels with 5x7 blocks:")
print("  - 5 columns x 7 rows of LED blocks")
print("  - Each block is 2x2 pixels")
print("  - Gap between blocks: 1 pixel")
print("  - Total: 5*2 + 4*1 = 14 wide, 7*2 + 6*1 = 20 tall")

# Verify the structure
block_w = 2
block_h = 2
gap = 1
cols = 5
rows = 7

total_w = cols * block_w + (cols - 1) * gap
total_h = rows * block_h + (rows - 1) * gap
print(f"\n  Block structure: {cols}x{rows} blocks, each {block_w}x{block_h} with {gap}px gap")
print(f"  Total size: {total_w}x{total_h} pixels")

# Map the character bitmap to block structure
print("\n\nMapping '0' character to 5x7 blocks:")
if 48 in chars:
    info = chars[48]
    char_img = img.crop((info['x'], info['y'], info['x'] + info['w'], info['y'] + info['h']))
    char_img = char_img.convert('L')
    pixels = list(char_img.getdata())
    
    # For each block, check if any pixel is lit
    block_map = []
    for block_row in range(rows):
        row_blocks = []
        for block_col in range(cols):
            # Calculate pixel range for this block
            start_x = block_col * (block_w + gap)
            start_y = block_row * (block_h + gap)
            
            is_lit = False
            for dy in range(block_h):
                for dx in range(block_w):
                    px = start_x + dx
                    py = start_y + dy
                    if px < info['w'] and py < info['h']:
                        if pixels[py * info['w'] + px] < 128:
                            is_lit = True
                            break
                if is_lit:
                    break
            
            row_blocks.append(is_lit)
        block_map.append(row_blocks)
    
    print(f"  Block map for '0':")
    for row in range(rows):
        row_str = "    "
        for col in range(cols):
            if block_map[row][col]:
                row_str += "█"
            else:
                row_str += "·"
        print(row_str)
