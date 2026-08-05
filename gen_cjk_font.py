#!/usr/bin/env python3
"""
Generate src/fonts/lv_font_cjk.c — the CJK font used by the watchface in
Chinese mode (weather line, lunar/solar-term line and the date line).

Metrics are deliberately identical to lv_font_montserrat_12
(line_height = 15, base_line = 3), which is the font used for the same rows in
English mode. Chinese rows therefore occupy exactly the same vertical space as
English rows and the layout does not have to change per language.

Two things this generator must get right, both of which were broken before:

1. Row padding vs. `lv_font_fmt_txt_dsc_t.stride`.
   Each glyph row is emitted padded to a whole byte ((box_w + 1) // 2 bytes at
   4 bpp).  LVGL only skips that padding when `stride != 0`; with `stride = 0`
   it reads the bitmap as one tight stream of nibbles, so every row of a glyph
   with an odd box_w is shifted half a pixel further than the one before it and
   the glyph turns into diagonal noise.  We emit `.stride = 1` ("rows are
   aligned to 1 byte"), which matches the layout written here.

2. Glyph coverage.
   A codepoint that is not in the cmap is drawn by LVGL as an empty rectangle
   (LV_USE_FONT_PLACEHOLDER), i.e. the classic "tofu" box.  CHAR_GROUPS below
   must therefore cover every Chinese character the application can display;
   verify_coverage() cross-checks it against the string literals in src/.
"""

import os
import re
import glob
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(HERE, "app", "src")
OUT_PATH = os.path.join(SRC_DIR, "fonts", "lv_font_cjk.c")

# WenQuanYi Zen Hei is hinted for small pixel sizes, which matters a lot for
# dense glyphs such as 露/霜/蛰 at this size. SimSun turns to mush below ~14 px.
TTF_PATH = "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"
TTF_INDEX = 0
FONT_SIZE = 13

BPP = 4
FONT_NAME = "lv_font_cjk"

# Match lv_font_montserrat_12 so ZH and EN rows share one layout.
LINE_HEIGHT = 15
BASE_LINE = 3

ASCII_START = 0x20
ASCII_END = 0x7E
ASCII_COUNT = ASCII_END - ASCII_START + 1

# Every Chinese character the UI can render, grouped by the data table it comes
# from. Keep in sync with locale.c, watchface.c and lunar_calendar.c.
CHAR_GROUPS = [
    ("数字/月份", "一二三四五六七八九十"),
    ("星期",     "星期日周"),
    ("天气",     "晴多云阴雨雪"),
    ("字段标签", "心率步数电池楼层卡路里"),
    ("日出日落", "日出落"),
    ("月相",     "新月上下弦满"),
    ("语言",     "中文"),
    ("天干",     "甲乙丙丁戊己庚辛壬癸"),
    ("地支",     "子丑寅卯辰巳午未申酉戌亥"),
    ("生肖",     "鼠牛虎兔龙蛇马羊猴鸡狗猪"),
    ("农历月",   "正冬腊闰年"),
    ("农历日",   "初廿"),
    ("节气",     "小寒大立春雨水惊蛰分清明谷夏满芒种至暑处白露秋霜降冬雪"),
    ("其它",     "天"),
    ("符号",     "℃"),   # U+2103，天气行的摄氏度
    # 天气行的风向箭头，8 个方向。对应 Segment34 LED 字体里的 'a'-'h' 字形
    # （见 watchface.c 的 wind_arrow()）：↑ ↗ → ↘ ↓ ↙ ← ↖。
    ("风向箭头", "↑↗→↘↓↙←↖"),
]


def build_char_lists():
    ascii_chars = [chr(c) for c in range(ASCII_START, ASCII_END + 1)]
    cjk = set()
    for _name, chars in CHAR_GROUPS:
        cjk.update(chars)
    return ascii_chars, sorted(cjk, key=ord)


def verify_coverage(cjk_chars):
    """Warn about non-ASCII characters that appear in a string literal under
    src/ but are not in CHAR_GROUPS — those would render as placeholder boxes."""
    have = set(cjk_chars)
    used = {}
    # 一次扫描里同时匹配字符串字面量和注释，只保留前者。注释必须排除：源码里
    # 中文注释经常用 ASCII 引号引一段话（"元素四角"、"风从哪来"），只按引号找
    # 字面量的话这些全会被当成待渲染文本报成缺字，真的缺字就淹在噪声里了。
    # 字符串分支写在最前面，所以字面量内部的 // 和 /* 不会被误当注释。
    pattern = re.compile(r'"((?:[^"\\\n]|\\.)*)"|/\*.*?\*/|//[^\n]*', re.S)
    for path in glob.glob(os.path.join(SRC_DIR, "**", "*.c"), recursive=True):
        if os.sep + "fonts" + os.sep in path:
            continue
        text = open(path, encoding="utf-8", errors="replace").read()
        for match in pattern.finditer(text):
            literal = match.group(1)
            if literal is None:      # 注释
                continue
            # locale.c stores its Chinese as \xNN escapes; fold those back first.
            decoded = re.sub(r"\\x([0-9a-fA-F]{2})",
                             lambda m: chr(int(m.group(1), 16)), literal)
            try:
                decoded = decoded.encode("latin1").decode("utf-8")
            except (UnicodeEncodeError, UnicodeDecodeError):
                pass
            for ch in decoded:
                if ord(ch) > ASCII_END:
                    used.setdefault(ch, os.path.relpath(path, HERE))

    missing = sorted(ch for ch in used if ch not in have)
    if missing:
        print("  WARNING: used in src/ but not in the font: " + "".join(missing))
        for ch in missing:
            print("    U+%04X %s  (%s)" % (ord(ch), ch, used[ch]))
    else:
        print("  coverage OK: every non-ASCII char in src/ string literals is present")
    return missing


def render_glyph(font, ch):
    """Render one glyph, 4 bpp, each row padded to a whole byte."""
    ascent, _descent = font.getmetrics()
    advance = font.getlength(ch)
    adv_w = max(1, int(round(advance * 16)))  # LVGL stores adv_w in 8.4 fixed point

    margin = 8
    baseline_y = margin + ascent
    img = Image.new("L", (int(advance) + 2 * margin + 8,
                          ascent + _descent + 2 * margin), 0)
    ImageDraw.Draw(img).text((margin, baseline_y), ch, font=font, fill=255, anchor="ls")

    bbox = img.getbbox()
    if bbox is None:  # blank glyph, e.g. space
        return {"box_w": 0, "box_h": 0, "ofs_x": 0, "ofs_y": 0,
                "adv_w": adv_w, "data": b""}

    x0, y0, x1, y1 = bbox
    box_w, box_h = x1 - x0, y1 - y0
    pixels = list(img.crop(bbox).getdata())

    row_bytes = (box_w + 1) // 2
    data = bytearray(row_bytes * box_h)
    for row in range(box_h):
        for col in range(box_w):
            nib = ((pixels[row * box_w + col] * 15 + 127) // 255) & 0x0F
            idx = row * row_bytes + col // 2
            if col % 2 == 0:
                data[idx] |= nib << 4
            else:
                data[idx] |= nib

    return {"box_w": box_w, "box_h": box_h,
            "ofs_x": x0 - margin, "ofs_y": baseline_y - y1,
            "adv_w": adv_w, "data": bytes(data)}


def build_cmaps(codepoints, first_gid):
    """Group consecutive codepoints into FORMAT0_TINY ranges."""
    ranges = []
    for i, cp in enumerate(codepoints):
        if ranges and cp == ranges[-1][0] + ranges[-1][1]:
            ranges[-1][1] += 1
        else:
            ranges.append([cp, 1, first_gid + i])
    return ranges


def check_vertical_fit(glyphs):
    """LVGL places a glyph at y = (line_height - base_line) - box_h - ofs_y
    inside the line box; anything outside 0..line_height gets clipped."""
    top_min, bot_max = LINE_HEIGHT, 0
    for g in glyphs:
        if g["box_h"] == 0:
            continue
        top = (LINE_HEIGHT - BASE_LINE) - g["box_h"] - g["ofs_y"]
        top_min = min(top_min, top)
        bot_max = max(bot_max, top + g["box_h"])
    status = "OK" if top_min >= 0 and bot_max <= LINE_HEIGHT else "CLIPPED"
    print("  vertical fit: rows %d..%d of 0..%d  -> %s"
          % (top_min, bot_max, LINE_HEIGHT, status))


def main():
    ascii_chars, cjk_chars = build_char_lists()
    print("Generating %s" % os.path.relpath(OUT_PATH, HERE))
    verify_coverage(cjk_chars)

    font = ImageFont.truetype(TTF_PATH, FONT_SIZE, index=TTF_INDEX)

    # Glyph 0 is the "not found" slot LVGL expects at index 0.
    glyphs = [{"box_w": 0, "box_h": 0, "ofs_x": 0, "ofs_y": 0, "adv_w": 0, "data": b""}]
    for ch in ascii_chars:
        glyphs.append(render_glyph(font, ch))
    cjk_first_gid = len(glyphs)
    for ch in cjk_chars:
        glyphs.append(render_glyph(font, ch))
    check_vertical_fit(glyphs)

    bitmap = bytearray()
    glyph_dsc_lines = []
    for g in glyphs:
        glyph_dsc_lines.append(
            "    {.bitmap_index = %d, .adv_w = %d, .box_w = %d, .box_h = %d,"
            " .ofs_x = %d, .ofs_y = %d},"
            % (len(bitmap), g["adv_w"], g["box_w"], g["box_h"], g["ofs_x"], g["ofs_y"]))
        bitmap.extend(g["data"])

    cjk_codepoints = [ord(ch) for ch in cjk_chars]
    cmap_ranges = build_cmaps(cjk_codepoints, cjk_first_gid)

    L = []
    L.append('#include "lv_font_cjk.h"')
    L.append('')
    L.append('/*')
    L.append(' * Auto-generated by gen_cjk_font.py — do not edit by hand.')
    L.append(' *')
    L.append(' * Source font : %s (index %d), %d px, %d bpp' % (TTF_PATH, TTF_INDEX, FONT_SIZE, BPP))
    L.append(' * Glyphs      : %d (1 placeholder + %d ASCII + %d CJK), %d bytes of bitmap'
             % (len(glyphs), len(ascii_chars), len(cjk_chars), len(bitmap)))
    L.append(' * Metrics     : line_height=%d, base_line=%d — same as lv_font_montserrat_12,'
             % (LINE_HEIGHT, BASE_LINE))
    L.append(' *               so Chinese rows line up with the English ones.')
    L.append(' * Bitmap      : 4 bpp,每行按字节对齐 -> .stride = 1 (必须；为 0 时 LVGL 会把')
    L.append(' *               奇数宽度的字形按紧凑位流解码，整个汉字会错位成噪点)')
    L.append(' */')
    L.append('')
    L.append('static const uint8_t %s_bitmap[%d] = {' % (FONT_NAME, len(bitmap)))
    for i in range(0, len(bitmap), 16):
        L.append('    ' + ', '.join('0x%02x' % b for b in bitmap[i:i + 16]) + ',')
    if not bitmap:
        L.append('    0')
    L.append('};')
    L.append('')
    L.append('static const lv_font_fmt_txt_glyph_dsc_t %s_glyph_dsc[] = {' % FONT_NAME)
    L.extend(glyph_dsc_lines)
    L.append('};')
    L.append('')
    L.append('static const lv_font_fmt_txt_cmap_t %s_cmap[] = {' % FONT_NAME)
    L.append('    /* ASCII 0x20-0x7E */')
    L.append('    {')
    L.append('        .range_start = 0x%02x, .range_length = %d, .glyph_id_start = 1,'
             % (ASCII_START, ASCII_COUNT))
    L.append('        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0,')
    L.append('        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY,')
    L.append('    },')
    L.append('    /* CJK — consecutive codepoints merged into one range each */')
    for i, (start, length, gid) in enumerate(cmap_ranges):
        comma = ',' if i < len(cmap_ranges) - 1 else ''
        L.append('    {')
        L.append('        .range_start = 0x%04x, .range_length = %d, .glyph_id_start = %d,'
                 % (start, length, gid))
        L.append('        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0,')
        L.append('        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY,')
        L.append('    }%s' % comma)
    L.append('};')
    L.append('')
    L.append('static const lv_font_fmt_txt_dsc_t %s_fmt_dsc = {' % FONT_NAME)
    L.append('    .glyph_bitmap = %s_bitmap,' % FONT_NAME)
    L.append('    .glyph_dsc = %s_glyph_dsc,' % FONT_NAME)
    L.append('    .cmaps = %s_cmap,' % FONT_NAME)
    L.append('    .kern_dsc = NULL,')
    L.append('    .kern_scale = 0,')
    L.append('    .cmap_num = %d,' % (1 + len(cmap_ranges)))
    L.append('    .bpp = %d,' % BPP)
    L.append('    .kern_classes = 0,')
    L.append('    .bitmap_format = 0,')
    L.append('    .stride = 1,   /* 每行按字节对齐，见文件头说明 */')
    L.append('};')
    L.append('')
    L.append('const lv_font_t %s = {' % FONT_NAME)
    L.append('    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,')
    L.append('    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,')
    L.append('    .line_height = %d,' % LINE_HEIGHT)
    L.append('    .base_line = %d,' % BASE_LINE)
    L.append('    .subpx = LV_FONT_SUBPX_NONE,')
    L.append('    .underline_position = -1,')
    L.append('    .underline_thickness = 1,')
    L.append('    .dsc = &%s_fmt_dsc,' % FONT_NAME)
    L.append('};')
    L.append('')

    with open(OUT_PATH, 'w', encoding='utf-8') as f:
        f.write('\n'.join(L))

    print("  glyphs   : %d (1 placeholder + %d ASCII + %d CJK)"
          % (len(glyphs), len(ascii_chars), len(cjk_chars)))
    print("  bitmap   : %d bytes" % len(bitmap))
    print("  cmap_num : %d (1 ASCII + %d CJK ranges)" % (1 + len(cmap_ranges), len(cmap_ranges)))
    print("  metrics  : line_height=%d, base_line=%d" % (LINE_HEIGHT, BASE_LINE))


if __name__ == '__main__':
    main()
