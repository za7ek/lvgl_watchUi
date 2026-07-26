# Segment34 Watchface 项目文档

## 1. 项目概览

| 项 | 说明 |
|---|---|
| 项目名 | Segment34 Watchface（段码式表盘） |
| 参考项目 | [Segment34.CN](https://github.com/laukeng/Segment34.CN)（Garmin Connect IQ 表盘） |
| 技术栈 | Zephyr RTOS v3.5.0 + LVGL v9.5.0 |
| 目标硬件 | Xiao BLE nRF52840（1MB Flash / 256KB RAM）+ GC9A01 圆形屏 240×240 |
| 模拟器 | native_sim（SDL），在 PC 上运行 |
| 依赖 | `west.yml` 管理 Zephyr/LVGL/zswatch 三个仓库 |

---

## 2. 目录结构

```
lgvl_watchUi/
├── app/
│   ├── CMakeLists.txt          # 构建配置，8 个源文件
│   ├── prj.conf                # 项目全局配置（LVGL/内核/硬件）
│   ├── Kconfig                 # 项目自定义 Kconfig
│   ├── west.yml                # West 依赖管理
│   ├── boards/
│   │   ├── native_sim.conf     # 模拟器覆盖配置
│   │   ├── native_sim.overlay  # 模拟器设备树覆盖
│   │   ├── xiao_ble.conf       # Xiao BLE 硬件配置
│   │   ├── xiao_ble.overlay    # Xiao BLE 设备树
│   │   ├── zswatch.conf        # ZSWatch 硬件配置
│   │   └── zswatch.overlay     # ZSWatch 设备树
│   ├── docs/
│   │   ├── DEVELOPMENT.md      # 开发文档
│   │   ├── HANDOVER.md         # 交接文档
│   │   └── PROJECT.md          # 项目总文档（本文件）
│   ├── scripts/
│   │   └── generate_font.py    # CJK 字体生成脚本
│   └── src/
│       ├── main.c              # 入口：初始化显示 + LVGL 主循环
│       ├── components/
│       │   ├── watchface.c/h   # 表盘主 UI（布局/定时器/数据更新）
│       │   ├── segment34.c/h   # 7 段数码管自定义绘制
│       │   ├── icons.c/h       # 10 种像素图标绘制
│       │   └── lunar_calendar.c/h  # 农历转换
│       ├── locale/
│       │   └── locale.c/h      # 中英双语国际化
│       ├── theme/
│       │   └── theme.c/h       # 6 套主题色方案
│       └── fonts/
│           └── lv_font_cjk.c/h # CJK 16px 字体
├── gen_cjk_font.py             # CJK 字体生成脚本（根目录）
└── .vscode/
    └── c_cpp_properties.json   # VS Code C/C++ 配置
```

---

## 3. 各模块详解

### 3.1 main.c — 程序入口

**文件**：`app/src/main.c`

```
main()
  ├── 获取显示设备 → display_blanking_off()
  ├── watchface_start()          ← 创建所有 UI 对象
  └── while(1) { lv_timer_handler(); k_msleep(5); }  ← 5ms 刷新
```

**关键注意事项**：
- LVGL 已由 Zephyr 的 SYS_INIT 阶段初始化完毕
- 应用层**不能**重复调用 `lv_init()` / `lv_display_create()` / `lv_theme_default_init()`
- 否则会创建第二个没有渲染缓冲的显示，导致 `lv_timer_handler()` 崩溃

---

### 3.2 watchface.c — 表盘主逻辑

**文件**：`app/src/components/watchface.c` / `watchface.h`

#### 布局坐标图（240×240 圆形屏幕）

```
Y=0   ┌─────────────────────────────────┐
      │  DAWN:     [moon]     DUSK:     │  Y=8 (FONT_LABEL 8px)
      │  01:18     [1QTR]     03:13     │  Y=22 (FONT_DATA 10px)
Y=40  │       59F, +4, 27%               │  (FONT_MED 12px)
Y=58  │       PARTLY CLOUDY              │  (FONT_MED 12px)
Y=82  │  ┃                              │  ← stress_bar (3×36px, 左侧)
      │  ┃   1 0 : 3 7                  │  ← segment34 时钟 (200×70)
      │  ┃                              │  ┃ ← bodybatt_bar (3×36px, 右侧)
Y=152 │  MON, 5 MAY 2025           32   │  ← date_label + seconds_label
Y=180 │  RECOVERY   LAST HR:  WEEK ACT  │  ← 3 个字段标签 (FONT_LABEL 8px)
      │   HRS:                         MIN:│
Y=192 │    5.0        80        0       │  ← 3 个字段值 (FONT_BIG 16px)
Y=224 │  ♥     08573     🔥            │  ← 图标 + 5位步数 + 图标
Y=246 │           [battery]             │  ← 电池图标
      └─────────────────────────────────┘
```

#### 布局常量

```c
#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

#define CLOCK_W 200
#define CLOCK_H 70
#define CLOCK_X 20
#define CLOCK_Y 82

#define FONT_LABEL  &lv_font_montserrat_8    // 小标签
#define FONT_DATA   &lv_font_montserrat_10   // 数据文字
#define FONT_MED    &lv_font_montserrat_12   // 中等文字
#define FONT_BIG    &lv_font_cjk_16          // 大数字（CJK字体）
```

#### API 函数

| 函数 | 作用 | 调用时机 |
|---|---|---|
| `watchface_start()` | 创建所有 UI 对象、启动定时器 | `main()` 中调用 |
| `watchface_stop()` | 删除定时器和 UI 对象 | 退出时 |
| `watchface_update_time()` | 更新时钟+冒号闪烁+秒数 | 每秒 |
| `watchface_update_date()` | 更新日期/月相/日出日落 | 每秒 |
| `watchface_update_weather()` | 更新温度+天气描述 | 每 10 秒 |
| `watchface_update_sensors()` | 更新心率/步数/卡路里/电池 | 每 10 秒 |
| `watchface_switch_language()` | 中英文切换 | 用户触发 |
| `watchface_switch_theme()` | 6 套主题循环切换 | 用户触发 |

#### 定时器

- `time_timer`：1000ms，更新时间+日期
- `sensor_timer`：10000ms，更新传感器模拟数据

#### UI 对象清单（共约 20 个 lv_obj_t）

| 对象 | 类型 | 位置 | 字体 | 颜色角色 |
|---|---|---|---|---|
| `root_page` | container | (0,0) 240×240 | - | bg |
| `dawn_label` | label | (10, 8) | montserrat_8 | field_lbl |
| `dawn_time_label` | label | (10, 22) | montserrat_10 | data_val |
| `moon_label` | label | (105, 12) | montserrat_10 | moon |
| `dusk_label` | label | (180, 8) | montserrat_8 | field_lbl |
| `dusk_time_label` | label | (180, 22) | montserrat_10 | data_val |
| `temp_label` | label | (0, 40) | montserrat_12 | text |
| `weather_label` | label | (0, 58) | montserrat_12 | weather |
| `segment_clock` | custom obj | (20, 82) 200×70 | - | clock_on / clock_off |
| `stress_bar` | rect obj | (15, 116) 3×36 | - | stress |
| `bodybatt_bar` | rect obj | (222, 116) 3×36 | - | bodybatt |
| `date_label` | label | (20, 160) | montserrat_10 | text |
| `seconds_label` | label | (198, 160) | montserrat_10 | data_val |
| `field1_label` | label | (6, 180) | montserrat_8 | field_lbl |
| `field1_value` | label | (6, 192) | cjk_16 | heart_rate |
| `field2_label` | label | (86, 180) | montserrat_8 | field_lbl |
| `field2_value` | label | (86, 192) | cjk_16 | steps |
| `field3_label` | label | (166, 180) | montserrat_8 | field_lbl |
| `field3_value` | label | (166, 192) | cjk_16 | accent |
| `bottom5_label` | label | (90, 224) | montserrat_12 | clock_on |
| 底部图标 ×3 | custom obj | 底部各位置 | - | 各角色 |

---

### 3.3 segment34.c — 数码管时钟

**文件**：`app/src/components/segment34.c` / `segment34.h`

#### 工作原理

```
segment34_init()
  → 创建 1 个 lv_obj_t
  → 注册 LV_EVENT_DRAW_MAIN 回调
  → segment34_draw_event_cb() 中绘制 4 位数字 + 冒号

segment34_set_time(hours, minutes)
  → 更新 digits[4] = {H/10, H%10, M/10, M%10}
  → lv_obj_invalidate() 触发重绘
```

#### 7 段掩码表

```c
static const uint16_t segment_masks_7seg[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,  // 0 1 2 3 4
    0x6D, 0x7D, 0x07, 0x7F, 0x6F   // 5 6 7 8 9
};
```

每位的 7 个段（a-g）位置计算：
- `sw = digit_w / 4`（段宽）
- `sh = digit_h / 7`（段高）
- 段 a (顶横)
- 段 b (右上竖)
- 段 c (右下竖)
- 段 d (底横)
- 段 e (左下竖)
- 段 f (左上竖)
- 段 g (中间横)

#### 数据结构

```c
typedef struct {
    lv_obj_t *obj;
    uint16_t width;
    uint16_t height;
    lv_color_t color_on;
    lv_color_t color_off;
    bool show_colon;
    uint8_t digits[4];  // [小时十位, 小时个位, 分钟十位, 分钟个位]
} segment34_t;
```

---

### 3.4 icons.c — 像素图标

**文件**：`app/src/components/icons.c` / `icons.h`

#### 工作原理

```
icon_draw(parent, ICON_HEART, x, y, color)
  → 创建 8×16 的 lv_obj_t
  → 用 icon_info_t 存储图标类型+颜色（lv_malloc 分配）
  → LV_EVENT_DRAW_MAIN 回调中逐像素绘制
```

#### 图标数据格式

每行 1 字节（8 像素宽），bit=1 画点，bit=0 不画。

#### 图标列表

| 枚举名 | 尺寸 | 用途 |
|---|---|---|
| ICON_HEART | 8×8 | 心率 |
| ICON_STEPS | 8×11 | 步数 |
| ICON_BATTERY_FULL | 8×8 | 电池满 |
| ICON_BATTERY_EMPTY | 8×8 | 电池空 |
| ICON_ALARM | 8×10 | 闹钟 |
| ICON_BLUETOOTH | 8×9 | 蓝牙 |
| ICON_MOON | 8×9 | 月相 |
| ICON_ARROW_UP | 8×9 | 上升箭头 |
| ICON_ARROW_DOWN | 8×9 | 下降箭头 |
| ICON_CALORIES | 8×9 | 卡路里 |

#### 添加新图标的步骤

1. 在 `icons.h` 的 `icon_t` 枚举中新增一项
2. 在 `icons.c` 中新增位图数据数组
3. 在 `get_icon_data()` 函数的 switch 中新增 case

---

### 3.5 theme.c — 主题系统

**文件**：`app/src/theme/theme.c` / `theme.h`

6 套主题 × 17 种颜色角色。

#### 主题列表

| 主题 | 时钟色 | 强调色 | 背景 |
|---|---|---|---|
| green | #00FF88 | #FFAA00 | #0A1628 |
| blue | #00AAFF | #FFAA00 | #0A1628 |
| red | #FF4444 | #FFAA00 | #1A0A10 |
| orange | #FFAA00 | #FF4444 | #1A100A |
| purple | 见代码 | 见代码 | 见代码 |
| cyan | 见代码 | 见代码 | 见代码 |

#### 颜色角色

| 角色 | 用途 |
|---|---|
| `bg` | 背景色 |
| `clock_on` | 时钟点亮段颜色 |
| `clock_off` | 时钟熄灭段颜色 |
| `text` | 普通文字 |
| `accent` | 强调色（卡路里等） |
| `weather` | 天气描述文字 |
| `heart_rate` | 心率数值 |
| `steps` | 步数数值 |
| `battery` | 电池图标 |
| `field_lbl` | 字段标签（暗色调） |
| `field_bg` | 字段背景 |
| `data_val` | 数据值文字（日出日落时间等） |
| `stress` | 压力条（左侧竖条） |
| `bodybatt` | 身体电量条（右侧竖条） |
| `notif` | 通知数 |
| `moon` | 月相文字 |
| `outline` | 边框 |

#### 切换主题

调用 `watchface_switch_theme()` 循环切换 6 套主题。

---

### 3.6 locale.c — 国际化

**文件**：`app/src/locale/locale.c` / `locale.h`

#### 工作原理

```c
locale_get_string(LOCALE_STR_MONDAY)
  → current_lang == LANG_EN ? "MON" : "星期一"
```

#### 字符串列表

- 星期：MON-SUN / 星期一-星期日
- 月份：JAN-DEC / 一月-十二月
- 天气：SUNNY/CLOUDY/OVERCAST/RAIN/SNOW / 晴/多云/阴/雨/雪
- 传感器：HR/STEPS/BATTERY/FLOORS/CAL / 心率/步数/电池/楼层/卡路里
- 日出日落：DAWN/DUSK / 日出/日落
- 月相：NEW/1QTR/FULL/3QTR / 新月/上弦/满月/下弦
- 语言：Chinese/English / 中文/英文

共 35 个字符串 ID，中英双语。**默认语言为 LANG_EN**。

---

### 3.7 lunar_calendar.c — 农历

**文件**：`app/src/components/lunar_calendar.c` / `lunar_calendar.h`

```c
lunar_calendar_convert(2026, 7, 25, &lunar)
  → lunar.year_name = "丙午年"
  → lunar.month_name = "六月"
  → lunar.day_name = "初二"
  → lunar.jieqi = "大暑"
```

#### API

| 函数 | 作用 |
|---|---|
| `lunar_calendar_convert()` | 公历转农历完整信息 |
| `lunar_get_year_name()` | 天干地支年名 |
| `lunar_get_month_name()` | 农历月名（含闰月标记） |
| `lunar_get_day_name()` | 农历日名 |
| `lunar_get_jieqi()` | 节气名称 |

---

## 4. 构建与运行

### 4.1 模拟器（PC 上快速预览）

```bash
# 编译
west build -b native_sim lgvl_watchUi/app --build-dir native_ui

# 运行
cd native_ui && ./zephyr.exe
```

### 4.2 真机（Xiao BLE nRF52840）

```bash
# 编译
west build -b xiao_ble lgvl_watchUi/app --build-dir build_xiao

# 烧录
west flash
```

---

## 5. 配置文件说明

| 文件 | 用途 |
|---|---|
| `app/prj.conf` | 全局配置：LVGL minimal、16bpp、字体 8/10/12/14、内存 32KB |
| `app/boards/native_sim.conf` | 模拟器：SDL 显示、日志 DEBUG 级 |
| `app/boards/xiao_ble.conf` | 真机：GC9A01 显示、传感器占位 |

### LVGL 关键配置（prj.conf）

```ini
CONFIG_LVGL=y
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_CONF_MINIMAL=y

# 布局引擎
CONFIG_LV_USE_FLEX=y
CONFIG_LV_USE_THEME_DEFAULT=y

# 字体
CONFIG_LV_FONT_MONTSERRAT_8=y
CONFIG_LV_FONT_MONTSERRAT_10=y
CONFIG_LV_FONT_MONTSERRAT_12=y
CONFIG_LV_FONT_MONTSERRAT_14=y

# 控件
CONFIG_LV_USE_LABEL=y
CONFIG_LV_USE_BAR=y

# 内存
CONFIG_LV_Z_MEM_POOL_SIZE=32768   # 32 KB LVGL 堆

# 渲染缓冲
CONFIG_LV_USE_DRAW_SW=y
CONFIG_LV_Z_VDB_SIZE=10            # 10% 屏幕大小
CONFIG_LV_Z_DOUBLE_VDB=y           # 双缓冲
```

### 启用更多字体

在 `prj.conf` 和对应 board 的 `.conf` 中添加：
```ini
CONFIG_LV_FONT_MONTSERRAT_16=y
CONFIG_LV_FONT_MONTSERRAT_20=y
```

---

## 6. 常见修改指南

| 想做什么 | 改哪个文件 | 改什么 |
|---|---|---|
| 调整布局位置 | `watchface.c` | `lv_obj_set_pos()` 的 x/y 参数 |
| 改字体大小 | `watchface.c` | `FONT_LABEL/FONT_DATA/FONT_MED/FONT_BIG` 宏 |
| 改颜色 | `theme.c` | 对应主题的 `theme_colors_t` 字段 |
| 加新图标 | `icons.c/h` | 新增 bitmap 数据 + enum + get_icon_data 分支 |
| 加新字符串 | `locale.h/c` | 新增 enum + zh_strings/en_strings 条目 |
| 加新 UI 元素 | `watchface.c` | `watchface_start()` 中创建，`watchface_update_*()` 中更新 |
| 启用更多字体 | `prj.conf` + `boards/*.conf` | `CONFIG_LV_FONT_MONTSERRAT_XX=y` |
| 调整内存 | `prj.conf` | `CONFIG_LV_Z_MEM_POOL_SIZE` |
| 改时钟样式 | `segment34.c` | `segment34_draw_event_cb()` 中的段绘制逻辑 |
| 切换默认语言 | `locale.c` | `current_lang = LANG_EN` 改为 `LANG_ZH` |
| 切换默认主题 | `watchface.c` | `theme_init(THEME_GREEN)` 改为其他主题 |

---

## 7. LVGL v9 注意事项

项目使用 LVGL v9.5.0，与 v8 有以下重要差异：

### 7.1 自定义绘制

**v8 方式**（不可用）：
```c
lv_obj_set_draw_cb(obj, my_draw_func);
lv_draw_ctx_t *draw_ctx = lv_draw_get_ctx(lv_obj_get_layer(obj));
```

**v9 方式**（当前项目使用）：
```c
lv_obj_add_event_cb(obj, my_draw_event_cb, LV_EVENT_DRAW_MAIN, NULL);
// 在回调中：
lv_layer_t *layer = lv_event_get_layer(e);
lv_draw_rect(layer, &dsc, &area);
```

### 7.2 颜色/样式 API

**v9 中** `lv_obj_get_style_bg_color()` 直接返回颜色，不需要传指针：
```c
lv_color_t color = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);  // 正确
lv_obj_get_style_bg_color(obj, LV_PART_MAIN, &color);            // 错误，参数过多
```

### 7.3 文字绘制函数签名

**v9 中** `lv_draw_label()` 只有 3 个参数，文字在 dsc.text 中：
```c
dsc.text = "hello";
lv_draw_label(layer, &dsc, &coords);
```

---

## 8. 所见即所得 IDE 推荐

### 8.1 SquareLine Studio（推荐）

- **官网**：https://squareline.io
- LVGL 官方推荐的可视化设计器
- 拖拽控件、设置样式、导出 C 代码
- 免费版支持基础功能，专业版支持更多控件
- **限制**：导出的代码是通用 LVGL 代码，需要手动适配 Zephyr 集成
- **注意**：自定义绘制的数码管和图标无法用 SquareLine 生成

### 8.2 VS Code + LVGL 插件

- 项目已有 `.vscode/` 和 `.clangd` 配置
- 安装 "LVGL" 扩展可获得代码高亮和基本预览
- 配合 clangd 做代码补全

### 8.3 NXP GUI Guider

- 免费，NXP 出品
- 基于 LVGL 的可视化设计器
- 导出 C 代码可用
- **限制**：偏 NXP 平台，需要手动调整适配

### 8.4 在线模拟器

- **LVGL Online Editor**: https://sim.lvgl.io
- 浏览器中写 LVGL 代码，即时预览
- 适合快速试验 UI 效果
- 不适合完整项目开发

### 推荐工作流

> **SquareLine Studio** 做布局原型设计 → 导出 C 代码 → 手动集成到 Zephyr 项目中

数码管时钟（segment34）和像素图标（icons）是自定义绘制，SquareLine 无法生成，只能手写。

---

## 9. 参考项目对比

参考项目 `Segment34.CN` 是 Garmin Connect IQ 表盘，使用 Monkey C 语言。

| 维度 | 参考项目（Garmin） | 本项目（Zephyr） |
|---|---|---|
| 平台 | Garmin Connect IQ | Zephyr RTOS + LVGL |
| 语言 | Monkey C | C |
| 大时钟 | segments80narrow 专用位图字体 | 代码绘制 7 段数码管 |
| 数据字体 | led / led_small 专用字体 | Montserrat 标准字体 |
| 图标 | icons / moon 位图字体 | 代码绘制像素图标 |
| 表圈刻度 | 代码绘制 | 未实现（可补） |
| 渐变效果 | gradient.png 图片叠加 | 未实现 |
| 资源格式 | .fnt + .png 位图字体 | LVGL 字体结构 + 代码绘制 |

两者平台完全不同，资源不可直接复用，需要在 LVGL 框架下重新实现等价效果。

python scripts/bmfont2lvgl.py lv_font_segments80 "C:\zheng\github\zarek\Segment34.CN-master\resources\fonts\segments80narrow.fnt" "~/zephyr-project/Segment34.CN/resources/fonts/segments80.png" src/fonts/lv_font_segments80.c