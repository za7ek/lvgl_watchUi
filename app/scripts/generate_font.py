#!/usr/bin/env python3
import subprocess
import os

CJK_CHARS = [
    '一', '二', '三', '四', '五', '六', '七', '八', '九', '十',
    '月', '日', '年', '初', '廿', '正', '冬', '腊',
    '甲', '乙', '丙', '丁', '戊', '己', '庚', '辛', '壬', '癸',
    '子', '丑', '寅', '卯', '辰', '巳', '午', '未', '申', '酉', '戌', '亥',
    '鼠', '牛', '虎', '兔', '龙', '蛇', '马', '羊', '猴', '鸡', '狗', '猪',
    '星', '期', '晴', '雨', '雪', '云', '雷', '雾',
    '心', '率', '步', '电', '量', '温', '度', '湿', '气', '压',
    '时', '间', '日', '期', '农', '历', '天', '气',
    '一', '二', '三', '四', '五', '六', '日',
    '月', '火', '水', '木', '金', '土', '日',
]

def generate_font():
    font_dir = os.path.dirname(os.path.dirname(__file__))
    font_path = os.path.join(font_dir, 'src', 'fonts')
    
    os.makedirs(font_path, exist_ok=True)
    
    chars_str = ''.join(CJK_CHARS)
    
    cmd = [
        'npx', 'lv_font_conv',
        '--size', '16',
        '--format', 'c',
        '--font', 'NotoSansSC-Regular.otf',
        '-o', os.path.join(font_path, 'lv_font_cjk.c'),
        '--bpp', '1',
        '--symbols', chars_str,
        '--no-compress',
        '--name', 'lv_font_cjk_16',
    ]
    
    print(f"Running: {' '.join(cmd)}")
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        print(result.stdout)
        if result.stderr:
            print(f"stderr: {result.stderr}")
        if result.returncode != 0:
            print(f"Error: Command failed with return code {result.returncode}")
            print("Please install lv_font_conv: npm install -g lv_font_conv")
            print("And download NotoSansSC-Regular.otf font file")
    except FileNotFoundError:
        print("Error: lv_font_conv not found")
        print("Please install it with: npm install -g lv_font_conv")

if __name__ == '__main__':
    generate_font()