# LVGL Watch UI 开发文档

## 1. 项目概述

### 1.1 项目简介

本项目是一个基于 LVGL (Light and Versatile Graphics Library) 的智能手表 UI 应用，运行在 Zephyr RTOS 上。项目采用 Segment34 风格的数码时钟设计，支持中文/英文双语切换和多种主题配色。

### 1.2 目标硬件

| 组件 | 型号 | 参数 |
|------|------|------|
| MCU | nRF52840 | 1MB Flash, 256KB RAM |
| 开发板 | Seeed XIAO BLE | nRF52840 Sense |
| 显示屏 | GC9A01 | 1.28英寸圆形, 240x240, 16位色深 |

### 1.3 软件架构

- **操作系统**: Zephyr RTOS v3.5.0
- **图形库**: LVGL v8.3.11
- **构建工具**: West/Zephyr Build System
- **开发语言**: C99

---

## 2. 项目结构

### 2.1 目录布局

```
lgvl_watchUi/
├── app/                              # 应用主目录
│   ├── CMakeLists.txt               # CMake 构建配置
│   ├── Kconfig                      # 应用 Kconfig 配置
│   ├── prj.conf                     # 默认配置文件
│   ├── west.yml                     # West 清单文件
│   ├── boards/                      # 板级配置
│   │   ├── native_sim.conf          # 模拟器配置
│   │   ├── native_sim.overlay       # 模拟器设备树覆盖
│   │   ├── xiao_ble.conf            # 真机配置
│   │   ├── xiao_ble.overlay         # 真机设备树覆盖
│   │   ├── zswatch.conf             # zswatch 默认配置
│   │   └── zswatch.overlay          # zswatch 设备树覆盖
│   ├── src/                         # 源代码目录
│   │   ├── main.c                   # 应用入口
│   │   ├── components/              # UI 组件
│   │   │   ├── segment34.c/h        # Segment34 数码时钟组件
│   │   │   ├── watchface.c/h        # 表盘主组件
│   │   │   └── lunar_calendar.c/h   # 农历转换组件
│   │   ├── fonts/                   # 字体文件
│   │   │   ├── lv_font_cjk.c/h      # CJK 中文字体
│   │   ├── locale/                  # 国际化支持
│   │   │   ├── locale.c/h           # 多语言字符串管理
│   │   └── theme/                   # 主题系统
│   │       ├── theme.c/h            # 主题颜色管理
│   └── scripts/                     # 辅助脚本
│       └── generate_font.py         # 字体生成脚本
└── .gitignore                       # Git 忽略配置
```

### 2.2 核心组件说明

| 组件 | 职责 | 关键文件 |
|------|------|----------|
| **Segment34** | 数码时钟显示，使用34段数码管风格 | `src/components/segment34.c` |
| **Watchface** | 表盘主界面，管理所有子组件 | `src/components/watchface.c` |
| **Lunar Calendar** | 公历转农历，支持中文日期显示 | `src/components/lunar_calendar.c` |
| **Theme** | 主题颜色管理，支持6种配色方案 | `src/theme/theme.c` |
| **Locale** | 多语言支持，中文/英文切换 | `src/locale/locale.c` |

---

## 3. 编译与运行

### 3.1 环境准备

```bash
# 进入项目目录
cd ~/zephyr-project

# 激活虚拟环境
source .venv/bin/activate

# 确保依赖已安装
west update
```

### 3.2 编译命令

#### 3.2.1 Native Simulator (SDL)

用于在 PC 上模拟运行，验证 UI 效果：

```bash
west build -b native_sim lgvl_watchUi/app --build-dir native_ui
```

编译产物位于：`~/zephyr-project/native_ui/zephyr/zephyr.exe`

#### 3.2.2 真机编译 (Xiao BLE)

```bash
west build -b xiao_ble lgvl_watchUi/app
```

编译产物位于默认构建目录：`build/zephyr/zephyr.hex`

### 3.3 运行模拟器

```bash
cd ~/zephyr-project/native_ui
./zephyr.exe
```

### 3.4 烧录到真机

```bash
west flash --hex-file build/zephyr/zephyr.hex
```

---

## 4. 配置说明

### 4.1 默认配置 (prj.conf)

```
CONFIG_LVGL=y                          # 启用 LVGL
CONFIG_LV_COLOR_DEPTH_16=y             # 16位色深
CONFIG_LV_CONF_MINIMAL=y               # 最小化配置

# LVGL 核心功能
CONFIG_LV_USE_FLEX=y                   # 启用 Flex 布局
CONFIG_LV_USE_THEME_DEFAULT=y          # 使用默认主题
CONFIG_LV_FONT_MONTSERRAT_14=y         # 启用 Montserrat 14号字体

# 启用的 Widgets
CONFIG_LV_USE_LABEL=y                  # 标签组件
CONFIG_LV_USE_BAR=y                    # 进度条组件

# 内存配置
CONFIG_LV_Z_MEM_POOL_SIZE=32768        # LVGL 堆大小 32KB
CONFIG_LV_Z_VDB_SIZE=10                # VDB 大小 10%
CONFIG_LV_Z_DOUBLE_VDB=y               # 双缓冲

# 应用配置
CONFIG_ZSWATCH_ROUND_SCREEN=y          # 圆形屏幕支持
CONFIG_SEGMENT34_SHOW_LUNAR=y          # 显示农历
CONFIG_SEGMENT34_LANGUAGE="zh"         # 默认语言
CONFIG_SEGMENT34_THEME="green"         # 默认主题
```

### 4.2 板级配置差异

#### native_sim.conf

```
CONFIG_SDL_DISPLAY=y                   # 使用 SDL 显示驱动
CONFIG_LV_Z_FLUSH_THREAD=y            # 启用刷新线程
```

#### xiao_ble.conf

```
CONFIG_DISPLAY=y                       # 启用显示
CONFIG_GC9A01=y                        # GC9A01 驱动
CONFIG_SPI=y                           # 启用 SPI
CONFIG_PM=y                            # 启用电源管理
```

### 4.3 设备树覆盖

#### native_sim.overlay

强制 SDL 显示为 240x240 分辨率，匹配真机：

```dts
&sdl_dc {
    width = <240>;
    height = <240>;
};
```

#### xiao_ble.overlay

配置 GC9A01 显示驱动的 SPI 连接：

```dts
&spi0 {
    gc9a01: display@0 {
        compatible = "solomon,gc9a01";
        spi-max-frequency = <40000000>;
        reset-gpios = <&gpio0 14 GPIO_ACTIVE_LOW>;
        cmd-data-gpios = <&gpio0 13 GPIO_ACTIVE_HIGH>;
        width = <240>;
        height = <240>;
    };
};
```

**硬件接线**：

| GC9A01 | XIAO BLE (nRF52840) |
|--------|---------------------|
| VCC    | 3V3                 |
| GND    | GND                 |
| DIN    | P0.10 (MOSI)        |
| CLK    | P0.09 (SCK)         |
| CS     | P0.11 (GPIO)        |
| DC     | P0.13 (GPIO)        |
| RST    | P0.14 (GPIO)        |
| BL     | P0.12 (GPIO)        |

---

## 5. 核心代码解析

### 5.1 main.c - 应用入口

```c
int main(void)
{
    LOG_INF("Segment34 Watchface starting...");
    watchface_start();                  // 初始化表盘
    
    while (1) {
        lv_timer_handler();             // LVGL 定时器处理
        k_msleep(5);                    // 5ms 延时
    }
    return 0;
}
```

**关键说明**：

- LVGL 由 Zephyr LVGL 模块通过 `lvgl_init()` 自动初始化（SYS_INIT）
- 无需手动调用 `lv_init()` / `lv_display_create()`
- 主循环每 5ms 调用一次 `lv_timer_handler()` 处理 UI 更新

### 5.2 watchface.c - 表盘组件

**初始化流程**：

1. `theme_init()` - 初始化主题
2. 创建根容器和主容器
3. 初始化 Segment34 时钟组件
4. 创建日期、农历、天气、心率、步数、电量等标签
5. 创建电量进度条
6. 启动定时器（1秒更新时间，10秒更新传感器）

**定时器回调**：

```c
static void time_update_callback(lv_timer_t *timer)
{
    watchface_update_time();            // 更新时间显示
    watchface_update_date();            // 更新日期显示
}

static void sensor_update_callback(lv_timer_t *timer)
{
    watchface_update_sensors();         // 更新传感器数据
}
```

### 5.3 segment34.c - 数码时钟组件

**设计特点**：

- 使用 4 个数字容器，每个容器包含 17 个段（共 68 段）
- 通过位掩码控制每段的亮灭
- 支持颜色配置（亮色/暗色）
- 冒号闪烁效果（每秒切换）

**段掩码定义**：

```c
static const uint16_t segment_masks[10] = {
    0x3FF, 0x060, 0x5DB, 0x5FB, 0x66B,  // 0-4
    0x7BB, 0x7FB, 0x0EB, 0x7FF, 0x6FB   // 5-9
};
```

### 5.4 lunar_calendar.c - 农历转换

**功能**：

- 支持 1900-2100 年的农历转换
- 使用查表法（`lunar_year_data`）计算农历日期
- 支持天干地支、生肖计算
- 生成中文日期字符串（如："正月十五"）

### 5.5 theme.c - 主题系统

**支持的主题**：

| 主题 | 名称 | 时钟颜色 |
|------|------|----------|
| THEME_GREEN | green | 绿色 |
| THEME_BLUE | blue | 蓝色 |
| THEME_RED | red | 红色 |
| THEME_ORANGE | orange | 橙色 |
| THEME_PURPLE | purple | 紫色 |
| THEME_CYAN | cyan | 青色 |

### 5.6 locale.c - 国际化

**支持语言**：

- 中文 (LANG_ZH)
- 英文 (LANG_EN)

**可本地化字符串**：

- 星期名称（星期一~星期日）
- 月份名称（一月~十二月）
- 天气描述（晴、多云、阴、雨、雪）
- 传感器名称（心率、步数、电池）

---

## 6. 内存优化策略

### 6.1 LVGL 内存配置

针对 nRF52840 的 256KB RAM 限制，采用以下优化：

1. **最小化 LVGL 配置**：`CONFIG_LV_CONF_MINIMAL=y`
2. **仅启用必要组件**：Label、Bar、Flex、Theme
3. **限制字体**：仅使用 Montserrat 14 和自定义 CJK 字体
4. **VDB 大小**：10% 屏幕（约 11.5KB），双缓冲约 23KB

### 6.2 Segment34 优化

原设计使用 70 个独立矩形对象，优化后：

- 使用 4 个数字容器（每容器 17 个子对象）
- 减少子对象数组重新分配的内存碎片
- 内存占用降低约 8 倍

### 6.3 内存预算

| 组件 | 内存占用 |
|------|----------|
| LVGL 堆 | 32KB |
| VDB 双缓冲 | ~23KB |
| 主栈 | 2KB |
| 工作队列栈 | 1KB |
| 堆内存池 | 2KB |
| **总计** | **~60KB** |

---

## 7. 开发指南

### 7.1 添加新功能

1. 在 `src/components/` 目录下创建新组件
2. 在 `CMakeLists.txt` 中添加源文件
3. 在 `prj.conf` 或板级配置中添加必要的 Kconfig 选项
4. 在 `watchface.c` 中集成新组件

### 7.2 添加新主题

在 `src/theme/theme.c` 的 `themes[]` 数组中添加新主题定义：

```c
{
    .name = "your_theme",
    .bg = LV_COLOR_MAKE(0xRR, 0xGG, 0xBB),
    .clock_on = LV_COLOR_MAKE(0xRR, 0xGG, 0xBB),
    .clock_off = LV_COLOR_MAKE(0xRR, 0xGG, 0xBB),
    .text = LV_COLOR_MAKE(0xRR, 0xGG, 0xBB),
    .accent = LV_COLOR_MAKE(0xRR, 0xGG, 0xBB),
    .weather = LV_COLOR_MAKE(0xRR, 0xGG, 0xBB),
    .heart_rate = LV_COLOR_MAKE(0xRR, 0xGG, 0xBB),
    .steps = LV_COLOR_MAKE(0xRR, 0xGG, 0xBB),
    .battery = LV_COLOR_MAKE(0xRR, 0xGG, 0xBB),
},
```

### 7.3 字体生成

运行字体生成脚本添加新的中文字符：

```bash
cd lgvl_watchUi/app/scripts
python3 generate_font.py
```

**注意**：需要安装 `lv_font_conv` 和 `NotoSansSC-Regular.otf` 字体文件。

### 7.4 调试技巧

```bash
# 启用详细日志
west build -b native_sim lgvl_watchUi/app -- -DCONFIG_LOG_DEFAULT_LEVEL=4

# 查看编译配置
cat native_ui/zephyr/.config

# 查看内存使用
west build -b native_sim lgvl_watchUi/app -- -DCONFIG_MEM_STATS=y
```

---

## 8. 常见问题

### 8.1 SDL 显示驱动问题

**现象**：模拟器运行时窗口无法打开

**解决方案**：
1. 确保系统已安装 SDL2 库
2. 在 `native_sim.conf` 中启用 `CONFIG_SDL_DISPLAY=y`
3. 检查 `native_sim.overlay` 中的分辨率配置

### 8.2 GC9A01 显示问题

**现象**：真机屏幕无显示或显示异常

**解决方案**：
1. 检查 SPI 接线是否正确
2. 确认 `xiao_ble.overlay` 中的 GPIO 配置与实际接线一致
3. 检查 SPI 频率设置（最大支持 40MHz）
4. 确保电源电压稳定（建议使用 3.3V）

### 8.3 内存不足

**现象**：运行时崩溃或 UI 刷新异常

**解决方案**：
1. 减少 LVGL 堆大小（`CONFIG_LV_Z_MEM_POOL_SIZE`）
2. 减少 VDB 大小（`CONFIG_LV_Z_VDB_SIZE`）
3. 避免创建过多 LVGL 对象
4. 及时清理不再使用的对象

### 8.4 编译错误

**现象**：找不到头文件或符号未定义

**解决方案**：
1. 确保 `west update` 已执行
2. 检查 `CMakeLists.txt` 中的源文件和包含路径
3. 确认 Kconfig 选项已正确配置

---

## 9. 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2026-07-22 | 初始版本，Segment34 时钟 + 基础 UI |
| v1.1 | 2026-07-23 | 添加农历支持 + 主题系统 + 国际化 |

---

## 附录：Kconfig 选项清单

```
# 应用专用选项
CONFIG_SEGMENT34_SHOW_LUNAR        # 显示农历
CONFIG_SEGMENT34_LANGUAGE          # 默认语言 (zh/en)
CONFIG_SEGMENT34_THEME             # 默认主题
CONFIG_SEGMENT34_ROUND_SCREEN      # 圆形屏幕支持
CONFIG_ZSWATCH_ROUND_SCREEN        # ZSWATCH 圆形屏幕支持
```