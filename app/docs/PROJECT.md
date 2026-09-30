# Segment34 Watchface 项目文档

> 架构总览。配置/内存/调试细节见 [DEVELOPMENT.md](DEVELOPMENT.md)，字体见 [FONTS.md](FONTS.md)，
> 农历数据见 [LUNAR.md](LUNAR.md)，上手与接线见 [HANDOVER.md](HANDOVER.md)。

## 1. 项目概览

| 项 | 说明 |
|---|---|
| 项目名 | Segment34 Watchface（段码式表盘） |
| 参考项目 | [Segment34.CN](https://github.com/laukeng/Segment34.CN)（Garmin Connect IQ 表盘，Monkey C） |
| 技术栈 | Zephyr RTOS v4.4.1 + LVGL v9.5.0 |
| 目标硬件 | Seeed XIAO BLE nRF52840 Sense（1 MB Flash / 256 KB RAM）+ GC9A01 圆屏 240×240 |
| 模拟器 | `native_sim/native/64`（SDL），PC 上同分辨率运行 |
| 语言 | C99；字体/农历表由 Python 脚本离线生成 |

**关于 `app/west.yml`**：它写着 Zephyr v3.5.0 / LVGL v9.5.0，但当前 workspace 的
`.west/config` 指向的是 `zephyr/west.yml`，实际用的是 **Zephyr v4.4.1**。`app/west.yml`
目前不是生效的清单，改它不影响构建。

---

## 2. 目录结构

```
lgvl_watchUi/
├── README.md                   # 项目入口与文档索引
├── gen_cjk_font.py             # CJK 字体生成器（当前在用，见 FONTS.md §2）
├── app/
│   ├── CMakeLists.txt          # 11 个源文件 + 5 个 include 目录
│   ├── prj.conf                # 全局配置（LVGL/内核/显示）
│   ├── Kconfig                 # 只 source Kconfig.zephyr；自定义选项已全部删除
│   ├── west.yml                # 非生效清单，见上
│   ├── boards/
│   │   ├── native_sim_native_64.conf     # 模拟器覆盖配置
│   │   ├── native_sim_native_64.overlay  # 强制 SDL 为 240×240
│   │   ├── xiao_ble_nrf52840_sense.conf     # 真机配置
│   │   ├── xiao_ble_nrf52840_sense.overlay  # GC9A01 over MIPI-DBI SPI
│   │   ├── zswatch.conf / zswatch.overlay   # ZSWatch（未验证）
│   ├── docs/                   # 本目录
│   ├── scripts/
│   │   ├── bmfont2lvgl.py      # BMFont→LVGL（4bpp，生成 segments80）
│   │   └── generate_font.py    # 旧 CJK 路线（lv_font_conv），已被 gen_cjk_font.py 取代
│   └── src/
│       ├── main.c              # 入口：解除 blanking + LVGL 主循环
│       ├── components/
│       │   ├── watchface.c/h   # 表盘全部 UI：布局、渲染分层、缓存、定时器、状态输入
│       │   ├── settings.c/h    # 运行时设置面板（语言/主题/月相/电池，LVGL overlay）
│       │   └── lunar_calendar.c/h  # 公历→农历/节气（查表，2026-2056）
│       ├── locale/locale.c/h   # 中英双语字符串表
│       ├── theme/theme.c/h     # 8 套主题 × 17 个颜色角色
│       └── fonts/              # 6 个自定义字体（见 §5）
└── tools/                      # 字体与农历表的生成/校验脚本
```

**注意：`segment34.c` 和 `icons.c` 已经不存在了。** 早期版本用自绘 7 段数码管和自绘
像素图标，现在两者都换成了位图字体（`lv_font_segments80` / `lv_font_icons`），旧文档里
提到的这两个模块、`segment34_set_time()`、`icon_draw()` 等 API 都已删除。

---

## 3. 屏幕布局（240×240 圆屏）

所有坐标都是 `watchface.c` 里的宏算出来的，改宏就改布局；越出圆形可视区会**编译不过**
（见 §4.2）。

```
      ┌───────────────────────────────────┐
 y=10 │            [🌙 20×20]             │  moon_label（图片模式，字形 '0'-'7'）
 y=11 │   DAWN:                  DUSK:    │  xsmol 10px
 y=19 │   01:18                  03:13    │  montserrat_10
 y=30 │        15~17℃, ↗4, 27%           │  temp_label（CJK 字体，含 ℃ 和箭头）
 y=45 │        PARTLY CLOUDY (40%)        │  weather_label（中文模式这行是农历）
      │  ┃                             ┃  │  stress_bar / bodybatt_bar（3px 宽）
 y=60 │  ┃    ██ ██  :  ██ ██          ┃  │  时钟 5 列，x=23..216，h=80
      │  ┃                             ┃  │
y=144 │      MON, 5 MAY 2025        32    │  date_label + seconds_label
y=160 │  RECOVERY   LAST HR:  WEEK ACT    │  三个字段标签（xsmol 10px）
      │  HRS:                      MIN:   │
y=173 │    5.0        80         0        │  三个 LED 点阵数值（3 格 × 16px）
y=198 │   [A]   0 8 5 7 3   [ᛒ]           │  状态图标 + 5 位步数点阵
y=223 │            [▮▮▮  ]                │  电池图标 24×12 + 帽 3×6
      └───────────────────────────────────┘
```

### 关键几何常量

```c
#define SAFE_R 117          /* 圆形可视区半径，圆心 (119.5, 119.5) */

#define COL_W    42         /* 数字列宽（= segments80 字形宽） */
#define COLON_W  18         /* 冒号列收窄，这是把时钟塞进圆里代价最小的一刀 */
#define COL_GAP  2
#define CLOCK_W  194        /* 4*42 + 18 + 4*2 */
#define CLOCK_H  80
#define CLOCK_X  23         /* CENTER_X - CLOCK_W/2 */
#define CLOCK_Y  60

#define LED_DIGIT_W 16      /* LED 字体 adv_w：14px 字形 + 2px 间隙 */
#define LED_DIGIT_H 20
#define FIELD1_DIGITS 3     /* 三个数值字段各 3 格（4 格会被圆切掉） */
#define FIELD_GAP  16
#define BOTTOM5_DIGITS 5    /* 步数 5 位 */
#define ICON_W 22           /* 状态图标位；ICON_GAP=2 是往内收的间隙 */
```

### 字体宏

| 宏 | 指向 | 用在哪 |
|---|---|---|
| `FONT_LABEL` | `lv_font_xsmol`（10px, 1bpp） | DAWN/DUSK、三个字段标签 |
| `FONT_DATA` | `lv_font_montserrat_12` | 日期行、秒数 |
| `FONT_TIME_SMALL` | `lv_font_montserrat_10` | 日出日落时间 |
| `FONT_MED` | `lv_font_montserrat_12` | 天气描述行 |
| `FONT_CJK` | `lv_font_cjk`（13px, 4bpp） | 中文行、含 ℃/箭头的温度行 |
| `FONT_LED` | `lv_font_led`（14×20, 1bpp） | 三个字段数值 + 步数 |
| `FONT_MOON` / `FONT_MOON_IMAGE` | `montserrat_8` / `lv_font_moon`（20×20） | 月相文字 / 月相图 |
| `FONT_ICONS` | `lv_font_icons`（21px, 1bpp） | 闹钟/勿扰/蓝牙/久坐图标 |

`FONT_CJK` 的 `line_height=15 / base_line=3` 与 `montserrat_12` 完全一致，所以中英文行占
同样的垂直空间，布局不用按语言分支。

---

## 4. watchface.c — 全部 UI 逻辑

1400 行，是项目里唯一的 UI 模块。理解它就理解了整个表盘。

### 4.1 渲染分层：时钟为什么不是"画数字"

`lv_font_segments80` 的字形是**反相**的：不透明像素 = 非笔段。每一列时钟叠三层：

```
Layer 0  列容器背景     纯色 clock_on              →  笔段的底色（亮）
Layer 1  digit label   数字/':' 用 clock_off 画    →  盖住非笔段（暗）
Layer 2  grid  label   '#' 用黑色画                →  网格线与点阵纹理，压在最上面
```

结果：笔段处透出 Layer 0 的亮色，非笔段处是 Layer 1 的暗色，网格黑线覆盖全列。
换主题只需要改 Layer 0 的背景色和 Layer 1 的文字色，Layer 2 恒为黑。

LED 点阵字段（`led_char_create`）用同样的把戏，每格两层：bg label 画 `#`（35 个点全亮）
做底，val label 画数字盖住非笔段。数字位 bg 用白、val 用暗绿；空位 bg 用暗绿、val 是空格。

窄冒号列里塞 42px 宽的字形，靠 `LV_TEXT_ALIGN_CENTER + LV_LABEL_LONG_CLIP` 居中裁切——
网格是均匀重复的、冒号两点也在正中，裁掉的都是空边。

### 4.2 圆形可视区的编译期校核

```c
BUILD_ASSERT(FITS_IN_SAFE_CIRCLE(CLOCK_W, CLOCK_Y, CLOCK_Y + CLOCK_H - 1), ...);
BUILD_ASSERT(FITS_IN_SAFE_CIRCLE(FIELD_ROW_W, FIELD_VAL_Y, ...), ...);
BUILD_ASSERT(... 左图标 ...);  BUILD_ASSERT(... 右图标 ...);
```

判据是**元素的四个角**而不是宽度：同样宽的一行，越靠上/下越容易被圆切。全部按 2 倍坐标
做整数运算以避开圆心的半像素。改了列宽、`CLOCK_Y`、字段格数或 `ICON_GAP` 而越界的话，
直接编译失败并给出该往哪个方向改。运行期的抓帧校核方法见 DEVELOPMENT.md §7.4。

### 4.3 重绘抑制与分级缓存

`lv_label_set_text()` 不管内容变没变都会重新分配文本、重排版、invalidate 整个 label——
在 GC9A01 上就是一次真实的 SPI 刷屏。所以所有文本/颜色更新统一走两个 helper：

```c
static void label_set_text(lv_obj_t *label, const char *text);   /* 内容相同则 no-op */
static void label_set_color(lv_obj_t *obj, lv_color_t color);    /* 颜色相同则 no-op */
```

在此之上还有按变化频率分的三级缓存：

| 数据 | 变化频率 | 拦截方式 |
|---|---|---|
| 秒数 | 每秒 | 不拦 |
| 时钟数字 | 分位每分钟、时位每小时 | `label_set_text()` 相等判断 |
| 日期 / 月相 / 日出日落 | 每天 | `cached_day_key`（年×512+yday），换天前直接 return |
| 农历 + 节气 | 每天 | `lunar_day_key` + `lunar_line[64]` 字符串缓存 |
| 天气数据 | `WEATHER_REFRESH_MIN` = 30 分钟 | `weather_next_refresh` 时间戳 |

农历换算要线性扫 383 项月表 + 744 项节气表，务必只在换天时做一次。
切换语言/主题后调 `watchface_invalidate_cache()` 让全部缓存失效、整屏重算。

### 4.4 定时器

| 定时器 | 周期 | 回调做什么 |
|---|---|---|
| `time_timer` | 1000 ms | `watchface_update_time()` + `watchface_update_date()` |
| `sensor_timer` | 10000 ms | 刷新模拟数据 → `watchface_update_sensors()`（内部再调 weather/battery/icons） |

### 4.5 公开 API

```c
void watchface_start(void);              /* 建全部 UI + 启定时器；main() 调一次 */
void watchface_stop(void);               /* 删定时器 + 删 root_page */

void watchface_update_time(void);        /* 时钟数字 */
void watchface_update_date(void);        /* 秒数 + 日期/月相/日出日落（按天缓存） */
void watchface_update_weather(void);     /* 温度/风/湿度行 + 天气描述或农历行 */
void watchface_update_battery(void);     /* 电池填充宽度、颜色、百分比显示模式 */
void watchface_update_sensors(void);     /* 三字段 + 步数 + 压力/体能条，并调用上面三个 */
void watchface_update_icons(void);       /* 两个状态图标位 */

void watchface_switch_language(void);        /* ZH ↔ EN */
void watchface_switch_theme(void);           /* 8 套主题循环 */
void watchface_switch_battery_display(void); /* 不显示 → 内部 → 外部 */
void watchface_switch_moon_display(void);    /* 文字 ↔ 图片 */
```

这些 `switch_*` 由 `settings.c` 的设置面板按钮调用；也可以在 GPIO 按钮回调里直接调用（见 §4.8）。

### 4.8 settings.c — 运行时设置面板

LVGL overlay，挂在所有表盘内容之上，默认隐藏。

**触发方式**

- native_sim：在表盘上按住鼠标左键 **3 秒**弹出面板
- 真机（无触摸屏）：在 `main.c` GPIO 回调里调 `settings_show()`

**实现要点**

```
z-order（高→低）
─────────────────────────────────
s_panel（设置面板）         ← settings_init() 最后创建，始终最高
─────────────────────────────────
input_layer（全屏透明层）   ← 捕获长按/短按，不干扰表盘显示
─────────────────────────────────
root_page（表盘内容）
```

透明 `input_layer` 是必要的：`root_page` 的子对象（`clock_bg`、bar 等由 `lv_obj_create` 生成）
默认带 `LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE`，会吞掉事件；独立的透明层绕开这个问题。

长按阈值通过 `lv_indev_set_long_press_time(indev, 3000)` 统一设置（遍历所有已注册输入设备）。
长按松手不触发关闭，因为关闭用的是 `LV_EVENT_SHORT_CLICKED`（只在 < 3s 的短按结束时触发）。

**公开 API**

```c
void settings_init(void);       /* watchface_start() 末尾调一次 */
void settings_show(void);       /* 弹出面板 + 启动15s自动关闭计时器 */
void settings_hide(void);       /* 关闭面板 */
bool settings_is_visible(void);
```

**native_sim SDL 输入前提**（`boards/native_sim_native_64`）：
- `CONFIG_INPUT=y` + `CONFIG_INPUT_SDL_TOUCH=y` + `CONFIG_LV_Z_POINTER_INPUT=y`
- overlay 中加 `zephyr,input-sdl-touch` 和 `zephyr,lvgl-pointer-input` 两个 DT 节点

### 4.6 状态图标位

倒数第二行（步数点阵）左右各一个图标位。每个位置选一种**指示器**，具体画哪个字形——
或者什么都不画——由状态决定。这套结构照搬 Segment34 的 `icon1`/`icon2` + `getIconState()`。

| 指示器 | 显示条件 | 字形 |
|---|---|---|
| `ICON_SLOT_ALARM` | 闹钟数 > 0 | `A` |
| `ICON_SLOT_DND` | 勿扰开启 | `D` |
| `ICON_SLOT_BLUETOOTH` | 始终显示（断开时画暗色） | `L` |
| `ICON_SLOT_BLUETOOTH_OFF` | 仅断开时显示 | `L` |
| `ICON_SLOT_MOVE_BAR` | 久坐等级 1-5 | `N` `O` `P` `Q` `R` |
| `ICON_SLOT_NONE` | 从不显示 | — |

```c
watchface_set_icon_slots(ICON_SLOT_ALARM, ICON_SLOT_BLUETOOTH);   /* 默认值 */

/* 状态输入。目前由 sim_update_weather() 喂随机值；接真实数据源时改从
 * RTC 闹钟表 / BLE 连接回调 / 活动监测调这几个函数，渲染侧不用动。 */
watchface_set_alarm_count(1);
watchface_set_dnd(false);
watchface_set_phone_connected(true);
watchface_set_move_bar_level(0);        /* 0-5，越界自动收到最近的合法值 */
```

蓝牙断开在参考图集里是字形 `M`，但它与 `L` 逐像素相同，区别只是整个符文画成 85/255 的
灰度。1bpp 字体装不下这个区别，所以 `M` 不收进字体，断开状态改用同一个 `L` 配
`lv_color_mix(notif, bg, 85)` 的暗色。

### 4.7 数据来源现状

**所有传感器/天气/状态数据都是模拟值**，定义在 `watchface.c` 顶部的 `sim_*` 和 `state_*`
静态变量里，由 `sim_update_data()` / `sim_update_weather()` 每 10 秒随机扰动。时间取自
`time(NULL)`（真机上还没接 RTC）。接真实数据时替换这两个函数即可，渲染路径不用动。

模拟温度是华氏度，界面统一用 `fahrenheit_to_celsius()` 转成摄氏度显示（做了四舍五入
补偿，C 的整数除法是向零截断的）。

---

## 5. 字体

| 字体 | 规格 | 用途 | 生成器 |
|---|---|---|---|
| `lv_font_segments80` | 42×80, 4bpp | 大时钟（数字 + `:` + `#` 网格） | `app/scripts/bmfont2lvgl.py` |
| `lv_font_led` | 14×20, 1bpp | 字段数值 + 步数（反相极性） | `tools/gen_led_font_v3.py` **+ 手改** |
| `lv_font_xsmol` | 10px, 1bpp | 小标签 | `tools/gen_font.py` |
| `lv_font_battbar` | 1×8, 1bpp | 电池填充条（`\|` / `{`） | 手写，见 FONTS.md §10 |
| `lv_font_cjk` | 13px, 4bpp, 211 字形 | 中文行 + ℃/箭头 | `gen_cjk_font.py`（项目根） |
| `lv_font_moon` | 20×20, 1bpp, 9 字形 | 月相 `'0'`-`'7'` + 死星 `'8'` | `tools/gen_font.py` |
| `lv_font_icons` | 21px, 1bpp | 状态图标 `A/D/L/N-R` | `tools/gen_font.py` |

细节、必做手改和踩过的坑见 [FONTS.md](FONTS.md)。

---

## 6. theme.c — 主题系统

**8 套主题 × 17 个颜色角色**。枚举顺序与 `themes[]` 数组顺序必须严格一致
（历史上错位过一次，导致整屏配色乱套）。

```c
typedef enum {
    THEME_YELLOW,        /* 默认 */
    THEME_PINK,
    THEME_ORANGE_LIGHT,  /* 数组里 .name = "orange" */
    THEME_GREEN, THEME_BLUE, THEME_RED, THEME_PURPLE, THEME_CYAN,
    THEME_COUNT
} theme_color_t;
```

| 主题 | clock_on | bg |
|---|---|---|
| yellow（默认） | `#FFCA73` | `#080C14` |
| pink | `#FF99CC` | `#0C0814` |
| orange | `#FFB878` | `#0C0A08` |
| green / blue / red / purple / cyan | 见 `theme.c` | 见 `theme.c` |

17 个角色：`bg` `clock_on` `clock_off` `text` `accent` `weather` `heart_rate` `steps`
`battery` `field_lbl` `field_bg` `data_val` `stress` `bodybatt` `notif` `moon` `outline`。

**并非全部在用**：`accent`、`heart_rate`、`steps`、`field_bg`、`outline` 目前没有被
`watchface.c` 引用；字段标签和 DAWN/DUSK 用的是硬编码的 `#52AAAC`，LED 点阵用的是硬编码的
`LED_BG_COLOR` / `LED_FG_COLOR`，都不随主题变。这是已知的不一致，要做成主题化时把这些
硬编码换成角色引用即可。

默认主题在 `watchface_start()` 里写死为 `theme_init(THEME_YELLOW)`。

---

## 7. locale.c — 国际化

38 个字符串 ID × 中英双语，`locale_get_string(id)` 按 `current_lang` 取。
**默认语言是 `LANG_ZH`**（`locale.c` 里的 `current_lang` 初值）。

覆盖：星期、月份、天气描述（晴/多云/阴/雨/雪/局部多云）、传感器名、日出日落、月相四相、
语言名。

不走 locale 表的部分：`DAWN:` / `DUSK:` 和三个字段标签（`RECOVERY HRS:` 等）恒为英文——
它们用的 `lv_font_xsmol` 里没有汉字；中文日期行的"周X"也是在 `watchface.c` 里直接写的。

---

## 8. lunar_calendar.c — 农历与节气

```c
lunar_date_t lunar;
lunar_calendar_convert(2026, 7, 25, &lunar);
/* lunar.year_name = "丙午年"  month_name = "六月"  day_name = "初二"  jieqi = "大暑" */
```

查表实现，**支持 2026-2056**（超出范围返回空串）：

- `LUNAR_OFFSET_DAYS[383]` — 每个农历月首相对 2026-01-01 的天数偏移
- `LEAP_MONTH_OFFSETS[11]` — 哪些索引是闰月
- `SOLAR_TERMS_OFFSETS[744]` — 31 年 × 24 节气

表由 `tools/gen_lunar_tables.py` 用天文历（ephem 朔望 + 太阳黄经过 15° 点）生成，
`tools/verify_lunar_tables.py` 逐日校验。详见 [LUNAR.md](LUNAR.md)。

月相不走这张表：`watchface.c` 的 `get_moon_phase()` 用儒略日 + 朔望月整数运算独立算，
精度比参考实现高一个数量级（2026-2056 共 11315 天，偏差 0.6% vs 参考实现的 12.2%）。

---

## 9. main.c — 入口

```c
main()
  ├── DEVICE_DT_GET(DT_CHOSEN(zephyr_display)) → display_blanking_off()
  ├── watchface_start()
  └── while (1) { lv_timer_handler(); k_msleep(5); }
```

**两个必须知道的点：**

1. LVGL 由 Zephyr 的 LVGL 模块在 `SYS_INIT`（`INIT_LEVEL_APPLICATION`，早于 `main()`）
   里初始化完毕——包括 `lv_init()`、创建绑定到 Zephyr display 驱动的显示（含渲染缓冲）、
   应用默认主题、把 tick 接到 `k_uptime_get_32`。应用层**绝不能**再调
   `lv_init()` / `lv_display_create()` / `lv_theme_default_init()`，否则会创建第二个没有
   渲染缓冲的显示并成为默认显示，`lv_timer_handler()` 渲染时直接崩。
2. Zephyr 的显示默认是 blanked 的，不调 `display_blanking_off()` 的话 native_sim 只会
   开一个空窗口。

---

## 10. 构建与运行

```bash
cd ~/zephyr-project && source .venv/bin/activate

# 模拟器
west build -b native_sim/native/64 -d ~/zephyr-project/native_ui ~/zephyr-project/lgvl_watchUi/app
west build -t run -d ~/zephyr-project/native_ui

# 真机
west build -b xiao_ble/nrf52840/sense -d ~/zephyr-project/xiao_build ~/zephyr-project/lgvl_watchUi/app
west flash -d ~/zephyr-project/xiao_build
```

板级配置文件名必须与 board 名对应（`/` 换成 `_`）：`native_sim/native/64` →
`boards/native_sim_native_64.conf`，`xiao_ble/nrf52840/sense` →
`boards/xiao_ble_nrf52840_sense.conf`。名字对不上的话 Zephyr 会**静默忽略**该文件。

---

## 11. 常见修改指南

| 想做什么 | 改哪 | 改什么 |
|---|---|---|
| 调整某一行的位置 | `watchface.c` 顶部 | 对应的坐标宏；越界会被 `BUILD_ASSERT` 挡下 |
| 时钟变大/变小 | `watchface.c` + 字体 | `COL_W`/`COLON_W`/`CLOCK_H` 要与 `segments80` 字形尺寸匹配 |
| 改颜色 | `theme.c` | 对应主题的 `theme_colors_t` 字段（注意 §6 的硬编码色） |
| 加主题 | `theme.h` + `theme.c` | 枚举与数组**同时**加，顺序必须一致 |
| 加图标 | `tools/gen_font.py` + `watchface.c` | 从 `icons.png` 多提一个字形，再加 `icon_slot_t` 分支 |
| 加中文字符 | `gen_cjk_font.py` | 加进 `CHAR_GROUPS` 重新生成；不加就是空心方框 |
| 加新字符串 | `locale.h/c` | 枚举 + 中英两张表同步加 |
| 接真实传感器 | `watchface.c` | 替换 `sim_update_data()` / `sim_update_weather()` |
| 接按键切换设置 | `main.c` + GPIO | 在 GPIO 中断回调里调 `settings_show()`，见 §4.8 |
| 调 LVGL 内存 | `prj.conf` | `CONFIG_LV_Z_MEM_POOL_SIZE`（当前 64 KB，余量很薄，见 DEVELOPMENT.md §6） |

---

## 12. LVGL v9 注意事项

项目用 LVGL v9.5.0，与网上大量 v8 教程的差异：

```c
/* 自定义绘制：v8 的 lv_obj_set_draw_cb / lv_draw_get_ctx 都没了 */
lv_obj_add_event_cb(obj, cb, LV_EVENT_DRAW_MAIN, NULL);
lv_layer_t *layer = lv_event_get_layer(e);
lv_draw_rect(layer, &dsc, &area);

/* 样式读取直接返回值，不是出参 */
lv_color_t c = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);

/* 文字绘制只有 3 个参数，文字在 dsc.text 里 */
dsc.text = "hello";
lv_draw_label(layer, &dsc, &coords);
```

自绘曾经在这个项目里坑过一次：旧的 `icons.c` 把**局部坐标**当层坐标传给 `lv_draw_rect()`，
像素全落到屏幕左上角又被裁到对象自己的范围里，结果一个点都没画出来过——而且不报错。
现在整套图标改成了字体 + label，重绘抑制、换色、裁剪全是现成的。

---

## 13. 与参考项目的对应关系

`Segment34.CN` 是 Garmin Connect IQ 表盘（Monkey C），资源不能直接复用，但资源**文件**
（`.fnt` + `.png`）可以转换：

| 维度 | 参考项目 | 本项目 |
|---|---|---|
| 平台 / 语言 | Connect IQ / Monkey C | Zephyr + LVGL / C |
| 大时钟 | `segments80narrow` 位图字体 | 同一份 `.fnt`+`.png` 转成 `lv_font_segments80` |
| 数值 | `led` / `led_small` 字体 | `lv_font_led`（同源转换） |
| 图标 / 月相 | `icons` / `moon` 位图字体 | `lv_font_icons` / `lv_font_moon`（同源转换） |
| 小标签 | `xsmol` | `lv_font_xsmol`（同源转换） |
| 天气行拼法 | `joinFour()`，`", "` 分隔 | 同 |
| 风向箭头 | `getWind()` 量化成 8 方位 → `'a'`-`'h'` | 同算法，字形换成 Unicode 箭头 |
| 图标状态机 | `getIconState()` | `icon_slot_glyph()` |
| 5/4 死星彩蛋 | `moonPhase()` 里返回 `"8"` | 同（仅图片模式） |
| 表圈刻度 / 渐变叠加 | 有 | **未实现** |

源资源路径：`~/zephyr-project/Segment34.CN/resources/fonts/`。

---

## 14. 已知缺口

- 传感器、天气、闹钟、蓝牙状态**全是模拟值**
- 时间来自 `time(NULL)`，真机上未接 RTC
- 真机按键未接入（GPIO 回调调 `settings_show()` 的代码已预留，见 §4.8）
- 无低功耗（`CONFIG_PM` 未开）、无背光控制
- 农历表只到 2056 年
- 部分主题颜色角色未被使用（见 §6）
- 表圈刻度、渐变叠加未实现
