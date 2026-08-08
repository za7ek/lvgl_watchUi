# LVGL Watch UI 开发文档

> 架构与模块职责见 [PROJECT.md](PROJECT.md)，本文只讲**开发时要动手的部分**：
> 配置、内存、圆形可视区校核、性能、调试。

## 1. 环境与版本

| 项 | 版本 | 从哪确认 |
|------|------|----------|
| Zephyr RTOS | **v4.4.1** | `zephyr/VERSION` |
| LVGL | **v9.5.0** | `modules/lib/gui/lvgl/lv_version.h` |
| 构建工具 | West + CMake ≥3.20 + Ninja | — |
| 语言 | C99 | — |
| Python | 见 `.python-version`（pyenv），需 Pillow | 生成字体用 |

`app/west.yml` 里写的 Zephyr v3.5.0 **不是实际版本**——workspace 的 `.west/config` 指向
`zephyr/west.yml`，`app/west.yml` 当前不生效。读到版本冲突时以 `zephyr/VERSION` 为准。

### 目标硬件

| 组件 | 型号 | 参数 |
|------|------|------|
| MCU | nRF52840 | 1 MB Flash, 256 KB RAM |
| 开发板 | Seeed XIAO BLE nRF52840 Sense | board: `xiao_ble/nrf52840/sense` |
| 显示屏 | GC9A01 | 1.28" 圆形, 240×240, RGB565, MIPI-DBI over SPI |
| 模拟器 | native_sim + SDL | board: `native_sim/native/64` |

---

## 2. 编译与运行

```bash
cd ~/zephyr-project
source .venv/bin/activate
```

### 2.1 模拟器

```bash
west build -b native_sim/native/64 -d ~/zephyr-project/native_ui ~/zephyr-project/lgvl_watchUi/app
west build -t run -d ~/zephyr-project/native_ui
# 或直接跑： ~/zephyr-project/native_ui/zephyr/zephyr.exe
```

### 2.2 真机

```bash
west build -b xiao_ble/nrf52840/sense -d ~/zephyr-project/xiao_build ~/zephyr-project/lgvl_watchUi/app
west flash -d ~/zephyr-project/xiao_build
```

### 2.3 板级配置文件的命名

Zephyr 按 board 名去 `boards/` 里找同名 `.conf`/`.overlay`，`/` 换成 `_`：

| board | 配置文件 |
|---|---|
| `native_sim/native/64` | `boards/native_sim_native_64.conf` / `.overlay` |
| `xiao_ble/nrf52840/sense` | `boards/xiao_ble_nrf52840_sense.conf` / `.overlay` |

**名字对不上会被静默忽略**——不报错、不警告，只是配置没生效。改完 board 名记得同步改文件名。

### 2.4 改了 Kconfig / overlay 之后

```bash
west build -b <board> -d <builddir> <app> --pristine   # 配置类改动务必 pristine
```

---

## 3. 配置说明

### 3.1 prj.conf（全局）

```ini
CONFIG_LVGL=y
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_CONF_MINIMAL=y          # 从"全关"起步，只开用得上的，省 ~100 KB Flash

CONFIG_LV_TXT_ENC_UTF8=y          # 必须！见 §3.4
CONFIG_LV_USE_FLEX=y
CONFIG_LV_USE_THEME_DEFAULT=y
CONFIG_LV_FONT_MONTSERRAT_8=y     # 8/10/12/14 都在用
CONFIG_LV_FONT_MONTSERRAT_10=y
CONFIG_LV_FONT_MONTSERRAT_12=y
CONFIG_LV_FONT_MONTSERRAT_14=y

CONFIG_LV_USE_LABEL=y             # 只用 label + bar + image
CONFIG_LV_USE_BAR=y
CONFIG_LV_USE_IMAGE=y

CONFIG_LV_Z_MEM_POOL_SIZE=65536   # 64 KB，余量很薄，见 §6
CONFIG_LV_USE_DRAW_SW=y
CONFIG_LV_Z_VDB_SIZE=10           # 10% 屏幕 ≈ 11.5 KB/缓冲
CONFIG_LV_Z_DOUBLE_VDB=y

CONFIG_MAIN_STACK_SIZE=8192
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=2048
CONFIG_HEAP_MEM_POOL_SIZE=0       # 内核堆一处没用到，留着白占 RAM
CONFIG_DISPLAY=y
```

三个**看起来该开其实无效**的选项，别再加回来：

- `CONFIG_LV_MEM_SIZE_KILOBYTES` — 只在 `LV_USE_BUILTIN_MALLOC` 时生效。本项目走的是
  `LV_STDLIB_CUSTOM`，这个值写多少都没用。
- `CONFIG_LV_USE_ASSERT_*` — 依赖 `CONFIG_ASSERT`。只开前者的话 `LV_ASSERT_HANDLER`
  映射到的 `__ASSERT_NO_MSG()` 会被整句编译掉，等于没开。
- `CONFIG_SENSOR` — 目前没有任何传感器驱动被使能（BMI270/BMP581 都还注释着），
  开着只是把 sensor 子系统白链进来。

### 3.2 native_sim_native_64.conf

在 prj.conf 之上：`CONFIG_SDL_DISPLAY=y`、`CONFIG_LV_Z_FLUSH_THREAD=y`（优先级 0）、
日志级别 4，以及开发期护栏 `CONFIG_ASSERT=y` + `CONFIG_LV_USE_ASSERT_MALLOC=y`——
LVGL 申请不到内存时给一句明确断言，而不是让 `lv_draw_label.c` 拿着 NULL 往下写
（表现为莫名其妙的段错误）。

**SDL 鼠标输入**（运行时设置面板的前提）：
```ini
CONFIG_INPUT=y
CONFIG_INPUT_SDL_TOUCH=y      # Zephyr SDL 触摸/鼠标输入驱动
CONFIG_LV_Z_POINTER_INPUT=y   # LVGL 从 DT 的 lvgl-pointer-input 节点读指针事件
```
对应 overlay 里需要两个 DT 节点（见 PROJECT.md §4.8）：
```dts
input_sdl_touch: input-sdl-touch { compatible = "zephyr,input-sdl-touch"; display = <&sdl_dc>; };
lvgl_pointer { compatible = "zephyr,lvgl-pointer-input"; input = <&input_sdl_touch>; display = <&sdl_dc>; };
```
**漏掉这些配置的现象**：SDL 窗口打开正常，但鼠标点击/长按没有任何反应，日志里
`sdl_input: Init 'input-sdl-touch' device` 这行不出现。

overlay 把 SDL 显示强制成 240×240，保证布局和缓冲尺寸在模拟器里就按真实分辨率验证。

### 3.3 xiao_ble_nrf52840_sense.conf / .overlay

```ini
CONFIG_DISPLAY=y
CONFIG_GC9X01X=y        # 注意驱动开关名，不是 GC9A01
CONFIG_SPI=y
CONFIG_MIPI_DBI=y
CONFIG_GPIO=y
CONFIG_SERIAL=y / CONFIG_UART_CONSOLE=y
CONFIG_CLOCK_CONTROL_NRF=y
CONFIG_LV_Z_VDB_SIZE=10 / CONFIG_LV_Z_DOUBLE_VDB=y
# CONFIG_LV_Z_FLUSH_THREAD is not set    # flush 在主循环做，省一个线程栈
```

overlay 里有三个**踩过的坑**，改之前先读注释：

1. `compatible` 必须是 `galaxycore,gc9x01x`，不是 `solomon,gc9a01`（后者在 Zephyr 里
   压根不存在，会让 `DT_CHOSEN(zephyr_display)` 解析不到节点，`main.c` 直接编译失败）。
   这个绑定还要求挂在 `zephyr,mipi-dbi-spi` 节点下，不能直接挂 `&spi0`。
2. 用 `&xiao_spi`（spi2），不是 `spi0`——nRF52840 的 spi0 与 uart0 共用 SERIAL0 实例，
   而 uart0 已被板级 dtsi 用作控制台。
3. 引脚必须选排针实际引出的（D0-D10）。旧文档写的 P0.09-P0.14 一个都没引出来，
   其中 P0.09/P0.10 还是 NFC 焊盘。

已从配置里**删掉**的两块，需要时看注释再加：USB Device + CDC ACM（约 20 KB Flash，
控制台走 UART 用不着，且旧 USB 栈在 Zephyr 4.4 已 deprecated）、`CONFIG_PM`
（没有设备做 PM 状态管理，开着只会引入 idle 切换逻辑和 Kconfig 告警）。

### 3.4 中文显示为方框？

`CONFIG_LV_CONF_MINIMAL=y` 会让 `LV_TXT_ENC` 默认成 ASCII，LVGL 把 UTF-8 的每个字节当成
独立码位 —— 一个汉字 = 3 个找不到的码位 = 3 个空心方框。**`prj.conf` 和板级 `.conf`
都要显式写 `CONFIG_LV_TXT_ENC_UTF8=y`**。

方框的另一种成因是字符不在字体的 cmap 里，那是字体问题，见 [FONTS.md](FONTS.md) §2.2 的
覆盖率自检。

### 3.5 应用自定义 Kconfig：已全部删除

`app/Kconfig` 现在只 `source "Kconfig.zephyr"`。此前定义过
`ZSWATCH_ROUND_SCREEN` / `SEGMENT34_SHOW_LUNAR` / `SEGMENT34_LANGUAGE` /
`SEGMENT34_THEME` / `SEGMENT34_ROUND_SCREEN` 五个选项，但 `src/` 里一次都没引用——
默认语言写死在 `locale.c`，默认主题写死在 `watchface_start()`，圆屏布局是硬编码的
240×240。留着只会让人以为改配置能生效。要做成可配置项时连同代码一起加回来。

---

## 4. 圆形可视区校核

GC9A01 是圆屏：帧缓冲 240×240 是方的，但只有以 (119.5, 119.5) 为心、R=120 的圆内能看见，
四角物理上不存在。`watchface.c` 里 `SAFE_R = 117`（离屏边留 3px 给表壳），
**任何一个绘制像素到圆心的距离都不许超过它**。

判据是元素的**四个角**，不是宽度——同样宽的一行，越靠近上下边缘越容易被切。
一行内容宽 W、最低一行在 y，则要求 `hypot(W/2, |y - 119.5|) ≤ SAFE_R`。

### 4.1 编译期（免费，改布局时自动生效）

`watchface.c` 有一组 `BUILD_ASSERT` 覆盖时钟块、三字段数值行、左右两个图标位。
全部按 2 倍坐标做整数运算以避开圆心的半像素：

```c
#define FITS_IN_SAFE_CIRCLE(w, y0, y1) \
    ((w)*(w) + DY2_MAX(y0,y1)*DY2_MAX(y0,y1) <= (2*SAFE_R)*(2*SAFE_R))
```

越界会编译失败，并在消息里给出该往哪个方向改（收窄 `COL_W`、减 `FIELD*_DIGITS`、
减 `ICON_GAP`……）。

### 4.2 运行期（改完文本内容之后）

`BUILD_ASSERT` 管不了**文本实际渲染出来有多宽**——中文行、长天气描述都可能超出预期。
改完别靠眼睛看，抓帧算：

```bash
# 1) 给 main.c 临时加一个 lv_snapshot_take() → PPM 的转储函数，在 lv_timer_handler()
#    跑够几十轮后调用一次；额外配置 CONFIG_LV_USE_SNAPSHOT=y 和
#    CONFIG_LV_Z_MEM_POOL_SIZE=524288（快照要一整屏 ARGB8888 缓冲）。
west build -b native_sim/native/64 -d ../native_snap ../lgvl_watchUi/app \
    -- -DEXTRA_CONF_FILE=/tmp/snap.conf
./../native_snap/zephyr/zephyr.exe

# 2) 对每个非背景像素算到圆心的距离，取最大值
python3 - <<'EOF'
from PIL import Image
import math
im = Image.open('/tmp/ui_zh.ppm').convert('RGB'); px = im.load(); bg = px[0, 0]
worst = max((math.hypot(x - 119.5, y - 119.5), (x, y))
            for y in range(240) for x in range(240) if px[x, y] != bg)
print('worst needR = %.1f at %s' % worst)
EOF
```

中英文各抓一张：中文行（农历/日期）比英文宽，两边都要过。

实测记录：中文天气行加上 `" (40%)"` 降水概率后宽度到 181px，而这一行 (y=32) 处圆的可用
宽度只有 155px，两头都会压到表圈上——所以中文模式不显示降水概率。

---

## 5. 性能：重绘抑制

`lv_label_set_text()` 不管内容变没变都会 `lv_free` + `lv_malloc` 一份文本、重新排版并
invalidate 整个 label——在 GC9A01 上就是一次真实的 SPI 刷屏。`lv_obj_set_style_text_color()`
同样不做相等判断。

表盘上绝大多数内容一分钟、一天甚至更久才变一次，所以**所有文本/颜色更新都必须走**
`watchface.c` 里的两个 helper：

```c
label_set_text(label, text);    /* 内容相同 → no-op */
label_set_color(obj, color);    /* 颜色相同 → no-op */
```

在此之上还有按天/按半小时的分级缓存（`cached_day_key`、`lunar_day_key`、
`weather_next_refresh`），见 PROJECT.md §4.3。这套组合把每秒重绘量降了约 85%：
每秒真正重绘的只有秒数标签，分位每分钟一次，其余大多一天一次。

**加新 UI 元素时的规矩**：更新函数里不要直接调 `lv_label_set_text()`；如果新数据一天只变
一次，把它挂到 `cached_day_key` 的分支里去。切换语言/主题后记得让缓存失效
（`watchface_invalidate_cache()`），否则整屏不会重算。

---

## 6. 内存

### 6.1 预算

| 组件 | 占用 |
|------|------|
| LVGL 堆（`LV_Z_MEM_POOL_SIZE`） | 64 KB |
| VDB 双缓冲（10% × 240×240 × 2B × 2） | ~23 KB |
| 主栈 | 8 KB |
| 系统工作队列栈 | 2 KB |
| 内核堆 | 0（无人使用） |
| **合计** | **~97 KB** / 256 KB |

### 6.2 LVGL 堆为什么是 64 KB，别往下调

除了 obj/label/样式，绘制时还要临时申请字形的 A8 缓冲。`segments80` 是 42×80，
**单个字形就要 42×96 = 4 KB**，整块时钟一次全屏重绘要同时开 10 个 label 的缓冲，峰值很高。

实测：**40 KB 会在首次全屏重绘时 OOM**——`lv_draw_buf_create_ex()` 返回 NULL，而
`lv_draw_label.c:662` 不判空，直接段错误；44 KB 才刚够。余量太薄，保持 64 KB 不动。
要压这一项得先换掉 42×80 的大字形字体。

开发时在 native_sim 上把 `CONFIG_ASSERT=y` + `CONFIG_LV_USE_ASSERT_MALLOC=y` 打开，
OOM 会变成一句明确断言而不是段错误。

### 6.3 省内存的正确方向

1. 换更小的时钟字体（这是最大头）
2. 减少同时存在的 LVGL 对象——目前光 LED 点阵就有 (3×3 + 5) × 2 = 28 个 label
3. `CONFIG_LV_Z_VDB_SIZE` 可以降到 5%，代价是 flush 次数翻倍

---

## 7. 开发指南

### 7.1 添加新组件

1. 在 `src/components/` 建文件
2. 加进 `app/CMakeLists.txt` 的 `target_sources(app PRIVATE ...)`
3. 需要新 include 路径的话加进 `target_include_directories`
4. 在 `watchface.c` 里创建对象并挂到某个 `watchface_update_*()` 上

### 7.2 添加新主题

`theme.h` 的枚举和 `theme.c` 的 `themes[]` 数组**必须同时改，且顺序严格一致**——
历史上错位过一次（枚举加了新主题但数组没动），结果整屏配色乱套且没有任何编译报错。

```c
{
    .name = "your_theme",
    .bg = LV_COLOR_MAKE(0xRR, 0xGG, 0xBB),
    .clock_on = ..., .clock_off = ..., .text = ..., .accent = ...,
    .weather = ..., .heart_rate = ..., .steps = ..., .battery = ...,
    .field_lbl = ..., .field_bg = ..., .data_val = ...,
    .stress = ..., .bodybatt = ..., .notif = ..., .moon = ..., .outline = ...,
},
```

注意有几个颜色是**硬编码**的、不随主题变（字段标签/DAWN/DUSK 的 `#52AAAC`、
LED 点阵的 `LED_BG_COLOR`/`LED_FG_COLOR`、时钟网格层的黑色）。见 PROJECT.md §6。

### 7.3 字体

全部生成脚本、必做手改和坑见 [FONTS.md](FONTS.md)。最常见的两件事：

- **加中文字符**：编辑项目根 `gen_cjk_font.py` 的 `CHAR_GROUPS`，重跑。脚本会扫
  `app/src/**/*.c` 里的字符串字面量做覆盖率自检，用到但没收进字体的汉字会打印告警。
- **`lv_font_led.c` 重新生成后必须做 3 处手改**（FONTS.md §5.3），否则数字会整体偏移一位。

### 7.4 状态图标位

见 PROJECT.md §4.6。改 `ICON_GAP` 时注意方向：它是**往里收的间隙**，加大它会把图标推向
外侧、更容易越界。这一行在整块表盘最靠下的位置，外侧下角是最容易被圆切掉的地方。
`watchface.c` 里有对应的 `BUILD_ASSERT` 兜底。

### 7.5 调试技巧

```bash
# 提高日志级别（native_sim 默认已是 4）
west build -b native_sim/native/64 -d ../native_ui ../lgvl_watchUi/app -- -DCONFIG_LOG_DEFAULT_LEVEL=4

# 查看最终生效的配置（排查"这个 CONFIG 到底开没开"）
grep LV_Z_MEM ~/zephyr-project/native_ui/zephyr/.config

# 查看 Flash/RAM 占用
west build -t rom_report -d ~/zephyr-project/xiao_build
west build -t ram_report -d ~/zephyr-project/xiao_build

# 真机串口（115200）
minicom -D /dev/ttyACM0 -b 115200
```

`watchface_start()` 里有一串 `printk()` 进度打点（`theme initialized` → `clock created`
→ … → `settings initialized`）。启动崩溃时看它停在哪一句，就知道是哪一步炸的。

### 7.6 运行时设置面板

设置面板（`settings.c`）在运行时切换语言/主题/月相/电池显示，不需要重新编译。

**native_sim 操作**：在 SDL 窗口内按住鼠标左键 **3 秒** → 面板弹出 → 点击对应按钮 →
短按面板外区域或等 **15 秒** 无操作后自动关闭。

**真机接入**：目前 GPIO 按钮代码未写入，在 `main.c` 的 GPIO 中断回调里调
`settings_show()` 即可；`settings.h` 已经公开该 API。

**常见问题**

| 现象 | 原因 | 解决 |
|---|---|---|
| 长按无反应 | SDL 鼠标驱动未启用 | 见 §3.2，确认 `CONFIG_INPUT_SDL_TOUCH=y` 和 DT 节点 |
| 松手面板立即消失 | 长按松手触发了 CLICKED | 已修复：关闭用 `LV_EVENT_SHORT_CLICKED`，不触发于长按松手 |
| 面板按钮文字乱码 | 默认字体不含汉字 | 已修复：按钮标签改为纯英文 |

---

## 8. 常见问题

### 8.1 模拟器窗口打不开

1. `sudo apt-get install libsdl2-dev`
2. 确认 `boards/native_sim_native_64.conf` 里有 `CONFIG_SDL_DISPLAY=y`
3. 确认文件名与 board 名匹配（§2.3）——名字错了会被静默忽略

### 8.2 窗口开了但是空的 / 透明

`main.c` 必须调 `display_blanking_off()`。Zephyr 的显示默认是 blanked 的。

### 8.3 中文显示成空心方框

见 §3.4 —— 先查 `CONFIG_LV_TXT_ENC_UTF8`，再查字体覆盖率。

### 8.4 启动就段错误

多半是 LVGL OOM。开 `CONFIG_ASSERT=y` + `CONFIG_LV_USE_ASSERT_MALLOC=y` 复现，
看是不是 `LV_Z_MEM_POOL_SIZE` 被调小了（§6.2）。

另一种可能：应用层调了 `lv_init()` / `lv_display_create()` / `lv_theme_default_init()`。
**绝对不能调**，理由见 PROJECT.md §9。

### 8.5 真机黑屏 / 花屏

1. 对照 [HANDOVER.md](HANDOVER.md) §4 逐针核对接线（引脚在 2026-08 改过一次）
2. 确认 `CONFIG_GC9X01X=y`（不是 `GC9A01`）
3. 降 `mipi-max-frequency`（当前 32 MHz）
4. 串口看有没有 `Segment34 Watchface starting...`

### 8.6 编译报找不到头文件/符号

1. `west update`
2. 检查 `CMakeLists.txt` 的 `target_sources` / `target_include_directories`
3. 配置类改动之后加 `--pristine`

### 8.7 数字显示成 N-1 / 每个字段都是错数

LED 字体的 `glyph_id_start` 基数没改对，见 FONTS.md §5.3 末尾的校验值。

---

## 9. 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2026-07-22 | 初始版本：自绘 7 段时钟 + 基础 UI |
| v1.1 | 2026-07-23 | 农历 + 主题系统 + 国际化 |
| v1.2 | 2026-07 下旬 | 自绘时钟/图标换成 BMFont 转换的位图字体（`segments80`/`led`/`xsmol`） |
| v1.3 | 2026-08-01 | 月相图片字体；农历/节气表扩到 2026-2056 |
| v1.4 | 2026-08-05 | 中文显示修复（UTF-8 + CJK 字体）；分级缓存减少 85% 重绘；布局收进圆形可视区 |
| v1.5 | 2026-08-06 | 天气行加风向/湿度/降水概率；步数行两侧加状态图标位 |
| v1.6 | 2026-08-07 | 运行时设置面板（settings.c）：长按3s弹出，支持语言/主题/月相/电池切换；native_sim SDL鼠标输入接入 |
