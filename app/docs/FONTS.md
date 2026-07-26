# 字体生成工具使用说明

本项目包含多个字体生成脚本，用于将不同来源的字体转换为 LVGL 兼容的 C 数组格式。

---

## 1. 字体生成脚本概览

| 脚本 | 用途 | 依赖 | 输出 |
|------|------|------|------|
| `gen_cjk_font.py` | 生成 CJK 中文字体 | Python + Pillow | `lv_font_cjk.c` |
| `generate_font.py` | 生成 CJK 中文字体（备选） | Node.js + lv_font_conv | `lv_font_cjk.c` |
| `bmfont2lvgl.py` | 转换 BMFont 数码管字体 | Python + Pillow | `lv_font_segments80.c` |

---

## 2. gen_cjk_font.py（CJK 中文字体生成）

**位置：** 项目根目录 `gen_cjk_font.py`

使用 Python PIL (Pillow) 库从 TTF 字体文件手动渲染生成 CJK 字体。

### 2.1 前置条件

```bash
pip install Pillow
# 需要系统字体 simhei.ttf（Windows 自带，路径：C:\Windows\Fonts\simhei.ttf）
```

### 2.2 使用方法

```bash
python gen_cjk_font.py
```

### 2.3 特点

- 纯 Python 实现，无需 Node.js
- 4bpp（16级灰度）格式，显示效果更好
- 字体大小 16px，使用 Windows 的 `simhei.ttf`
- 手动渲染字符并提取位图数据
- 支持两段 cmap：ASCII (0x20-0x7E) + CJK 稀疏映射
- 输出路径直接指向 WSL：`\\wsl$\Ubuntu\home\zheng_fang\...`

### 2.4 关键参数

位于 `gen_cjk_font.py` 文件开头：

```python
TTF_PATH = r"C:\Windows\Fonts\simhei.ttf"   # 字体文件路径
FONT_SIZE = 16                                # 字体大小
BPP = 4                                       # 每像素位数
OUT_PATH = r"\\wsl$\Ubuntu\home\zheng_fang\zephyr-project\lgvl_watchUi\app\src\fonts\lv_font_cjk.c"
ASCII_START = 0x20
ASCII_END = 0x7E
CJK_STRINGS = [...]                           # 需要包含的 CJK 字符列表
FONT_NAME = "lv_font_cjk_16"
LINE_HEIGHT = 18
BASE_LINE = 2
```

### 2.5 修改字符集

编辑 `CJK_STRINGS` 列表，添加或删除需要的字符：

```python
CJK_STRINGS = [
    u"一二三四五六七八九十",
    u"月日年星期天气",
    u"晴多云阴雨雪",
    # 在此添加更多字符...
]
```

---

## 3. generate_font.py（CJK 中文字体生成 - 备选）

**位置：** `app/scripts/generate_font.py`

使用 `lv_font_conv` 工具（Node.js）从 TTF/OTF 字体文件生成 CJK 字体。

### 3.1 前置条件

```bash
npm install -g lv_font_conv
# 需要 NotoSansSC-Regular.otf 字体文件
```

### 3.2 使用方法

```bash
cd app/scripts
python generate_font.py
```

### 3.3 特点

- 使用官方 LVGL 字体转换工具
- 1bpp（黑白）格式
- 字体大小 16px
- 从 `NotoSansSC-Regular.otf` 提取指定的 CJK 字符
- 输出到 `src/fonts/lv_font_cjk.c`

### 3.4 缺点

- 需要 Node.js 环境和 lv_font_conv 工具
- 需要手动准备 OTF 字体文件
- 1bpp 黑白格式，显示效果不如 4bpp 灰度

---

## 4. bmfont2lvgl.py（BMFont 数码管字体转换）

**位置：** `app/scripts/bmfont2lvgl.py`

将 BMFont 格式字体（.fnt + .png）转换为 LVGL 兼容的 C 数组格式。主要用于转换数码管风格的字体。

### 4.1 前置条件

```bash
pip install Pillow
```

### 4.2 使用方法

```bash
cd app
python scripts/bmfont2lvgl.py <font_name> <fnt_file> <png_file> <output_c_file>
```

**示例（生成 segments80 数码管字体）：**

```bash
python scripts/bmfont2lvgl.py lv_font_segments80 \
    "C:\zheng\github\zarek\Segment34.CN-master\resources\fonts\segments80narrow.fnt" \
    "C:\zheng\github\zarek\Segment34.CN-master\resources\fonts\segments80.png" \
    src/fonts/lv_font_segments80.c
```

### 4.3 特点

- 将 BMFont（.fnt + .png）转换为 LVGL 字体
- 4bpp 灰度格式
- 支持自定义字体名称
- 自动生成头文件声明

### 4.4 转换过程中的关键处理

| 处理项 | 说明 |
|--------|------|
| 字形描述符字段顺序 | `bitmap_index, adv_w, box_w, box_h, ofs_x, ofs_y` |
| adv_w 单位 | 转换为 1/16 像素（BMFont 的 xadvance × 16） |
| base_line 计算 | `line_height - base`（LVGL 的 base_line 是 descent） |
| ofs_y 计算 | `base - yoffset - height`（字符底部相对于基线的偏移） |
| cmap range_length | `last_char - first_char + 1` |
| 灰度图处理 | 直接使用灰度值作为 alpha，不反转 |

---

## 5. 字体文件集成

### 5.1 添加字体到项目

1. 将生成的 `.c` 文件放入 `app/src/fonts/` 目录
2. 创建对应的 `.h` 头文件（参考 `lv_font_segments80.h`）：

```c
#ifndef LV_FONT_XXX_H
#define LV_FONT_XXX_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_font_t lv_font_xxx;

#ifdef __cplusplus
}
#endif

#endif
```

3. 在 `app/CMakeLists.txt` 中添加源文件：

```cmake
target_sources(app PRIVATE
    ...
    src/fonts/lv_font_xxx.c
)
```

4. 在使用字体的代码中包含头文件并引用：

```c
#include "lv_font_xxx.h"

lv_obj_set_style_text_font(label, &lv_font_xxx, LV_PART_MAIN);
```

### 5.2 注意事项

- 字体文件中**不要**使用 `#if LV_VERSION_CHECK()` 条件编译，否则可能导致链接错误
- `lv_font_fmt_txt_cmap_t` 结构体在不同 LVGL 版本中字段可能不同，参考项目中已有的字体文件（如 `lv_font_cjk.c`）保持一致
- 生成字体后需要在 WSL 端同步文件才能编译

---

## 6. 当前项目字体清单

| 字体文件 | 字体名称 | 用途 | 来源 |
|----------|----------|------|------|
| `lv_font_cjk.c` | `lv_font_cjk_16` | CJK 中文字体 | gen_cjk_font.py 生成 |
| `lv_font_segments80.c` | `lv_font_segments80` | 数码管时钟字体 | bmfont2lvgl.py 转换 |
