# 字体生成工具使用说明

本项目包含多个字体生成脚本，用于将不同来源的字体转换为 LVGL 兼容的 C 数组格式。

---

## 1. 字体生成脚本概览

| 脚本 | 位置 | 用途 | 依赖 | 输出 | 是否需手改 |
|------|------|------|------|------|-----------|
| `gen_cjk_font.py` | 项目根 | 生成 CJK 中文字体 | Python + Pillow | `lv_font_cjk.c` | 否 |
| `generate_font.py` | `app/scripts/` | 生成 CJK 中文字体（备选） | Node.js + lv_font_conv | `lv_font_cjk.c` | 否 |
| `bmfont2lvgl.py` | `app/scripts/` | 转换 BMFont 数码管字体 | Python + Pillow | `lv_font_segments80.c` | 否 |
| `gen_led_font_v3.py` | `tools/` | 生成 LED 点阵字体（5×7，1bpp） | Python + Pillow | `lv_font_led.c` | **是（必做，见 §5.3）** |
| `gen_font.py` | `tools/` | 通用 BMFont→LVGL 转换（1bpp，变宽） | Python + Pillow | `lv_font_xsmol.c` 等 | 否（已自动处理） |
| `verify_font.py` | `tools/` | 校验生成的字体 C 文件与源 PNG 是否一致 | Python + Pillow | 终端报告 | — |

> **运行环境注意（pyenv）**：项目根目录有 `.python-version`，pyenv 据此选择带 Pillow 的 Python。**必须在项目根目录运行脚本**（`cd /home/zheng_fang/zephyr-project/lgvl_watchUi` 后再执行），否则会回退到系统 Python（无 Pillow，报 `ModuleNotFoundError: No module named 'PIL'`）。

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

## 5. gen_led_font_v3.py（LED 点阵字体生成）

**位置：** `tools/gen_led_font_v3.py`

生成 `lv_font_led.c`——表盘 LED 点阵数值/标签使用的字体（`FONT_LED`，用于字段数值行与倒数第二行 steps）。源数据来自 BMFont 的 `led.fnt` + `led.png`（5×7 LED 块，每块 2×2 像素，块间 1px 间距，整体 14×20 像素）。

### 5.1 如何运行

脚本内路径是**硬编码**的，直接覆盖输出文件：

```bash
cd /home/zheng_fang/zephyr-project/lgvl_watchUi   # 必须在项目根（pyenv 需要）
python3 tools/gen_led_font_v3.py
```

硬编码路径（`main()` 开头，需要改路径时改这里）：

```python
fnt_path = '/home/zheng_fang/zephyr-project/Segment34.CN/resources/fonts/led.fnt'
png_path = '/home/zheng_fang/zephyr-project/Segment34.CN/resources/fonts/led.png'
output_path = '/home/zheng_fang/zephyr-project/lgvl_watchUi/app/src/fonts/lv_font_led.c'
```

### 5.2 如何修改

| 修改项 | 位置 | 说明 |
|--------|------|------|
| 字符集 | `needed_ids`（`main()` 内） | 默认 `[32,35,46,48..57,58]` = 空格、`#`、`.`、`0-9`、`:`。增删字符要同步改 `display_names` |
| 块布局 | 文件头 `BLOCK_COLS/ROWS/SIZE/GAP` | 5×7 块、2×2 像素、1px 间距，一般不动 |
| 合成字形 | `create_space_bitmap` / `create_hash_bitmap` / `create_dot_bitmap` | 空格=全透明；`#`=全 35 块亮（背景层）；`.`=col2-3/row5-6 四块（小数点） |
| 数字/冒号提取 | `extract_bitmap_14x20()` | 从 PNG 按 2×2 块采样，映射回 14×20 网格（见 §5.5 的 xoff 修复） |

> ⚠️ 运行后**直接覆盖** `lv_font_led.c`，且生成器输出与手改后的版本不同（见 §5.3）。**不要无条件提交生成器输出**。

### 5.3 ⚠️ 生成后必做的手改（防止遗漏，务必逐条执行）

`gen_led_font_v3.py` 输出的 `glyph_dsc` 和 `cmaps` 与 LVGL 实际要求不一致，**必须手改 3 处**（位图数据 `glyph_bitmap[]` 不用改）：

**手改 1 — 在 `glyph_dsc[]` 顶部插入保留项 `id=0`：**

生成器输出的 `glyph_dsc[]` 第一项是空格（0x0020），没有为 LVGL 保留 `glyph_id=0`。必须在数组最前面插入一项全 0 的保留项：

```c
// 生成器输出（错）：
static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {     0,  256,  14,  20,  0,  0 }  /* 0x0020 */,
    {    35,  256,  14,  20,  0,  0 }  /* 0x0023 */,
    ...

// 手改后（对）：
static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {     0,    0,   0,   0,  0,  0 }  /* id = 0 reserved */,
    {     0,  256,   0,   0,  0,  0 }  /* 0x0020 */,   ← 同时见手改 2
    {    35,  256,  14,  20,  0,  0 }  /* 0x0023 */,
    ...
```

**手改 2 — 空格（0x0020）的 `box_w`/`box_h` 改成 0：**

生成器把空格写成 14×20（`{ 0, 256, 14, 20, 0, 0 }`）。LVGL 会据此画占位框，必须改成 0×0：

```c
    {     0,  256,   0,   0,  0,  0 }  /* 0x0020 */,   // box_w=0, box_h=0
```

**手改 3 — 所有 `cmaps` 的 `glyph_id_start` 从 0 基改成 1 基：**

生成器用 `glyph_id_start = i`（0,1,2,...,13）。因为插入了保留项 `id=0`，每个字符的真实 glyph_id 都要 +1，改成 `1,2,3,...,14`：

```c
// 生成器输出（错）                  // 手改后（对）
.range_start = 32, ..., .glyph_id_start = 0,   →  1,
.range_start = 35, ..., .glyph_id_start = 1,   →  2,
.range_start = 46, ..., .glyph_id_start = 2,   →  3,
.range_start = 48, ..., .glyph_id_start = 3,   →  4,
.range_start = 49, ..., .glyph_id_start = 4,   →  5,
...                                              ...
.range_start = 58, ..., .glyph_id_start = 13,  →  14,
```

> `cmap_num = 14` 保持不变（cmap 条目数没变）。
>
> **校验**：改完后 `'0'`(0x0030) 的 `glyph_id_start` 应为 4、`'1'`(0x0031) 为 5、`':'`(0x003A) 为 14。若 `'1'` 误设成 4（和 `'0'` 撞），会出现"每个数字 N 显示成 N-1"。

**为什么不直接修生成器？** 这几项涉及 LVGL 的 glyph_id 0 占位约定，改生成器需同时调整 `glyph_dscs`/`cmap_entries` 的索引基线；为降低风险，当前选择"生成器产出 0 基 + 手改转 1 基"。`gen_font.py`（见 §6）已正确处理这些，无需手改。

### 5.4 位图布局与极性（调试时必读）

- **格式**：1bpp，bit-packed（行优先，每字节 MSB 在前），14px 宽 × 20px 高 = 280 bit = **每字形 35 字节**，**无行内字节对齐**。
- **极性**：`bit=1` = 不透明 = **非段**（被 val 标签用深绿色覆盖）；`bit=0` = 透明 = **亮段**（透出底层 `#` 标签的白色）。
- **渲染方式**（`watchface.c` 的 `led_char_create` + `led_field_set_value`）：每个字符位由两个 label 叠加——bg label 画 `#`（全 35 块不透明白）作底，val label 画数字（深绿）在上。结果：亮段=白，非段=深绿。
- **`#` 字形** = 全 35 块 `bit=1`（全不透明，作白色底）。
- **字形字节偏移**（`glyph_bitmap[]` 内）：空格=0、`#`=35、`.`=70、`0`=105、`1`=140、`2`=175、`3`=210、`4`=245、`5`=280、`6`=315、`7`=350、`8`=385、`9`=420、`:`=455（共 490 字节）。

### 5.5 近期修复：窄字符 xoffset bug（已修）

`extract_bitmap_14x20()` 早期用 `src_x = xoff + bc*3`（方向反），且把超出字形边界的块当成亮段，导致**窄字形**损坏：
- `'1'`（w=8, xoff=3）：丢了左上 serif 和底部 base，只剩一根竖线，渲染成深绿竖条 `#.###`（不是 `1`）。
- `':'`（w=2, xoff=6）：位图全 `0x00`（完全空白）。

全宽字形（w=14, xoff=0：`'0'`、`'2'`-`'9'`、`'#'`）不受影响。

**已修复**（`tools/gen_led_font_v3.py` 的 `extract_bitmap_14x20`）：改为 `raw_x = render_col - xoff`（字形放在 cell 的 xoff 处，减去 xoff 还原源像素），且 OOB 块（`tot==0`）视为背景非段。`lv_font_led.c` 的 `'1'`/`':'` 字节也已同步修正（偏移 140 / 455）。修复后 `'1'` 渲染为：
```
..#..    .##..    ..#..    ..#..    ..#..    ..#..    .###.
```

---

## 6. gen_font.py（通用 BMFont→LVGL 转换）

**位置：** `tools/gen_font.py`

通用的 BMFont（.fnt + .png）→ LVGL 1bpp 字体转换器，支持变宽字符、任意字号。用于生成 `lv_font_xsmol.c`（小标签字体，10px）等。

### 6.1 如何运行

命令行参数：`<fnt> <png> <output> <font_name> [char_ids...]`。不指定 `char_ids` 则生成 FNT 里的全部字符。

```bash
cd /home/zheng_fang/zephyr-project/lgvl_watchUi

# 生成 xsmol（小标签字体）
python3 tools/gen_font.py \
    /home/zheng_fang/zephyr-project/Segment34.CN/resources/fonts/xsmol.fnt \
    /home/zheng_fang/zephyr-project/Segment34.CN/resources/fonts/xsmol.png \
    app/src/fonts/lv_font_xsmol.c \
    lv_font_xsmol
```

`char_ids` 可用十进制（65）或十六进制（0x41）。例如只生成 ASCII 可见字符：

```bash
python3 tools/gen_font.py .../xsmol.fnt .../xsmol.png app/src/fonts/lv_font_xsmol.c lv_font_xsmol \
    0x20 0x21 0x22 ... 0x7E
```

### 6.2 特点

- 1bpp 黑白格式，自动极性检测（`detect_dark_background`，支持黑底白字 / 白底黑字 PNG）。
- **已自动正确处理** LVGL 约定：保留 `glyph_id=0` 占位项、`glyph_id_start` 用 1 基、空格 box 设为 0×0。**生成后无需手改**。
- `ofs_y = target_h - box_h - yoffset`（注意早期版本曾有 `+ yoffset` 的 bug，现版本已正确）。

### 6.3 如何修改

- 字号 / 行高：由 FNT 的 `lineHeight` 决定，脚本读取 `common lineHeight` 作为 `target_h`，一般不改。
- 字符集：通过命令行 `char_ids` 控制。
- 极性阈值：`extract_char_bitmap` 内 `pixel_val < 128` / `> 128`，如 PNG 反相可调。

---

## 7. verify_font.py（字体校验）

**位置：** `tools/verify_font.py`

对比生成的 C 字体文件与源 PNG，逐字形校验位图与 metrics（`adv_w`、`ofs_y`）是否一致。

### 7.1 如何运行

```bash
cd /home/zheng_fang/zephyr-project/lgvl_watchUi
python3 tools/verify_font.py
```

脚本末尾硬编码了校验列表（目前含 `xsmol`、`led_small`）。要校验其它字体，编辑 `verify_font()` 末尾的 `fonts` 列表：

```python
fonts = [
    ('xsmol',    f'{base}/Segment34.CN/resources/fonts/xsmol.fnt',
                 f'{base}/Segment34.CN/resources/fonts/xsmol.png',
                 f'{base}/lgvl_watchUi/app/src/fonts/lv_font_xsmol.c'),
    # 添加更多字体...
]
```

### 7.2 注意

- `verify_font.py` 只校验**位图数据**和 metrics，不校验 §5.3 的手改项（保留 glyph 0、glyph_id_start 基数）。`lv_font_led.c` 的手改需另行核对（见 §5.3 末尾的校验值）。
- LED 字体（`lv_font_led.c`）目前不在校验列表内，因为它的 `#`/`.`/空格是合成字形、且极性与 `gen_font.py` 相反。

---

## 8. 字体文件集成

### 8.1 添加字体到项目

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

### 8.2 注意事项

- 字体文件中**不要**使用 `#if LV_VERSION_CHECK()` 条件编译，否则可能导致链接错误
- `lv_font_fmt_txt_cmap_t` 结构体在不同 LVGL 版本中字段可能不同，参考项目中已有的字体文件（如 `lv_font_cjk.c`）保持一致
- 生成字体后需要在 WSL 端同步文件才能编译

---

## 9. 当前项目字体清单

| 字体文件 | 字体名称 | 用途 | bpp | 来源 |
|----------|----------|------|-----|------|
| `lv_font_cjk.c` | `lv_font_cjk_16` | CJK 中文字体 | 4 | gen_cjk_font.py 生成 |
| `lv_font_segments80.c` | `lv_font_segments80` | 数码管时钟字体 | 4 | bmfont2lvgl.py 转换 |
| `lv_font_led.c` | `lv_font_led` | 表盘 LED 点阵数值/标签（数值行、倒数第二行 steps） | 1 | gen_led_font_v3.py 生成 + §5.3 手改 |
| `lv_font_led_small.c` | `lv_font_led_small` | 日期行字体（13px） | 1 | gen_font.py 生成 |
| `lv_font_xsmol.c` | `lv_font_xsmol` | 标签行字体（10px） | 1 | gen_font.py 生成 |

> `lv_font_led.c` 每次用 `gen_led_font_v3.py` 重新生成后，**必须执行 §5.3 的 3 条手改**，否则会出现数字显示偏移 / 空格占位框 / LVGL 占位符渲染等问题。
