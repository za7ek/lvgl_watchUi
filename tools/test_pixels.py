#!/usr/bin/env python3
"""Test pixel values in LED font image"""

from PIL import Image

png_path = '/home/zheng_fang/zephyr-project/Segment34.CN/resources/fonts/led.png'
img = Image.open(png_path)

# Check the '0' character region (id=48, x=0, y=0, w=14, h=20)
char_img = img.crop((0, 0, 14, 20))
char_img = char_img.convert('L')
pixels = list(char_img.getdata())

print("Pixel values for '0' character (first 5 rows):")
for row in range(5):
    row_pixels = pixels[row*14:(row+1)*14]
    print(f"  Row {row}: {row_pixels}")

# Check the space character region (id=32, x=170, y=0, w=5, h=20)
space_img = img.crop((170, 0, 175, 20))
space_img = space_img.convert('L')
space_pixels = list(space_img.getdata())

print("\nPixel values for 'space' character:")
print(f"  First row: {space_pixels[:5]}")

# Let's check min and max values
print(f"\nMin pixel value: {min(pixels)}")
print(f"Max pixel value: {max(pixels)}")

# Check if the image uses inverted colors
# In BMFont, typically: 
# - White (255) = pixel is filled (foreground)
# - Black (0) = pixel is empty (background)
# But sometimes it's inverted:
# - Black (0) = pixel is filled
# - White (255) = pixel is empty

# Let's check by looking at the border of '0' character
print("\nChecking '0' character border (should be filled):")
print(f"  Top-left pixel (should be filled): {pixels[0]}")
print(f"  Top-right pixel: {pixels[13]}")
print(f"  Bottom-left pixel: {pixels[19*14]}")

# Check a middle pixel (should be empty)
print(f"  Middle pixel (should be empty): {pixels[10*14 + 7]}")
