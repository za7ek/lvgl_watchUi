#!/usr/bin/env python3
"""Check # character directly from PNG"""

from PIL import Image

png_path = '/home/zheng_fang/zephyr-project/Segment34.CN/resources/fonts/led.png'
img = Image.open(png_path)

# # character (id=35): x=890, y=0, w=14, h=20
x, y, w, h = 890, 0, 14, 20

char_img = img.crop((x, y, x + w, y + h))
char_img = char_img.convert('L')
pixels = list(char_img.getdata())

print(f"# character ({w}x{h}) - raw pixel values:")
for row in range(h):
    row_pixels = pixels[row*w:(row+1)*w]
    # Convert to visual: 255 = filled (white), 0 = empty (black)
    row_str = ''.join('█' if p > 128 else '·' for p in row_pixels)
    print(f"  Row {row:2d}: {row_str}")

print(f"\nMin value: {min(pixels)}, Max value: {max(pixels)}")

# Also check what the # character should look like - 
# it's used as background filler for the dot matrix display
# It should be a solid block of filled dots

# Let's also check the '0' character for comparison
print("\n\n'0' character ({w}x{h}) - raw pixel values:".format(w=14, h=20))
x0, y0 = 0, 0
char_img0 = img.crop((x0, y0, x0 + 14, y0 + 20))
char_img0 = char_img0.convert('L')
pixels0 = list(char_img0.getdata())

for row in range(20):
    row_pixels = pixels0[row*14:(row+1)*14]
    row_str = ''.join('█' if p > 128 else '·' for p in row_pixels)
    print(f"  Row {row:2d}: {row_str}")
