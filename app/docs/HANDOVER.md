# LVGL Watch UI 交接文档

> 本文档聚焦**换人接手、硬件接线、烧录、功能验证**。
> 开发细节见 [DEVELOPMENT.md](DEVELOPMENT.md)，架构见 [PROJECT.md](PROJECT.md)。

---

## 1. 项目概述

### 1.1 定位

基于 Zephyr RTOS + LVGL 的智能手表表盘，视觉对齐 Garmin Connect IQ 表盘
[Segment34.CN](https://github.com/laukeng/Segment34.CN)。目标硬件是
Seeed XIAO BLE (nRF52840) + GC9A01 1.28" 圆屏 240×240；开发时在 `native_sim`（SDL）
里跑同分辨率的模拟器。

### 1.2 主要功能

- Segment34 数码管风格大时钟（segments80 位图字体）
- 日出日落 / 月相 / 天气温度风向湿度
- 中文模式：农历日期 + 节气（2026-2056）
- 三字段 LED 点阵数值（RECOVERY HRS / LAST HR / WEEK ACT）+ 步数
- 状态图标（闹钟 / 勿扰 / 蓝牙 / 久坐）
- 电池图标
- 中英双语 / 8 套主题

### 1.3 硬件清单

| 组件 | 型号 |
|------|------|
| 开发板 | Seeed XIAO BLE nRF52840 Sense |
| 显示屏 | GC9A01 1.28" 圆形 240×240 |
| 连接线 | 杜邦线若干 |
| USB-C 线 | USB-C to USB-A |

---

## 2. 环境准备

### 2.1 系统依赖

```bash
sudo apt-get install libsdl2-dev minicom
```

### 2.2 Zephyr 开发环境

本项目使用已有的 `~/zephyr-project` workspace，不需要从零搭建：

```bash
cd ~/zephyr-project
source .venv/bin/activate   # Python 虚拟环境（west + 脚本依赖 Pillow）
west --version               # 确认 west 可用
```

| 组件 | 版本 |
|------|------|
| Zephyr RTOS | v4.4.1（`zephyr/VERSION`） |
| LVGL | v9.5.0（`modules/lib/gui/lvgl/lv_version.h`） |
| Zephyr SDK | v1.0.1 |
| CMake | ≥3.20 |
| Python | 见 `.python-version`（pyenv）|

---

## 3. 编译与运行

```bash
cd ~/zephyr-project && source .venv/bin/activate
```

### 3.1 模拟器（推荐先跑这个）

```bash
# 编译
west build -b native_sim/native/64 \
    -d ~/zephyr-project/native_ui \
    ~/zephyr-project/lgvl_watchUi/app

# 运行
west build -t run -d ~/zephyr-project/native_ui
# 或直接运行：~/zephyr-project/native_ui/zephyr/zephyr.exe
```

### 3.2 真机

```bash
# 编译
west build -b xiao_ble/nrf52840/sense \
    -d ~/zephyr-project/xiao_build \
    ~/zephyr-project/lgvl_watchUi/app

# 烧录
west flash -d ~/zephyr-project/xiao_build
```

### 3.3 改过 Kconfig / overlay 后必须 pristine

```bash
west build -b <board> -d <builddir> <app> --pristine
```

---

## 4. 硬件接线（GC9A01 → XIAO BLE 排针）

XIAO BLE 只引出 **D0-D10**，SPI 使用 xiao_spi（spi2）——nRF52840 的 spi0 与 uart0 共用
SERIAL0，板级 dtsi 已把 uart0 用作控制台，不能再用 spi0。

| GC9A01 引脚 | XIAO BLE 引脚 | nRF52840 GPIO | 功能 |
|-------------|---------------|----------------|------|
| VCC | 3V3 | — | 电源 3.3V |
| GND | GND | — | 地 |
| CLK | D8 | P1.13 | SPI SCK |
| DIN | D10 | P1.15 | SPI MOSI |
| CS | D3 | P0.29 | 片选（低有效） |
| DC | D6 | P1.11 | 数据/命令（高=数据） |
| RST | D7 | P1.12 | 复位（低有效） |
| BL | D5 | P0.05 | 背光（高=亮；可接 3V3 常亮） |

> ⚠️ **旧文档写的 P0.09-P0.14 全部错误**——这些引脚一个都没在排针上引出
> （P0.09/P0.10 还是 NFC 焊盘），按旧图接线屏幕必然不亮。

### 接线示意

```
GC9A01                  XIAO BLE
┌─────────┐            ┌───────────────────┐
│  VCC    │────────────│ 3V3               │
│  GND    │────────────│ GND               │
│  CLK    │────────────│ D8  (P1.13 SCK)   │
│  DIN    │────────────│ D10 (P1.15 MOSI)  │
│  CS     │────────────│ D3  (P0.29)       │
│  DC     │────────────│ D6  (P1.11)       │
│  RST    │────────────│ D7  (P1.12)       │
│  BL     │────────────│ D5  (P0.05)       │
└─────────┘            └───────────────────┘
```

---

## 5. 功能验证清单

### 5.1 模拟器验证

编译运行后应看到 240×240 的 SDL 窗口，逐项核对：

| # | 功能 | 预期现象 |
|---|------|---------|
| 1 | 大时钟显示 | 段码数字正常，冒号每秒闪烁 |
| 2 | 日期行 | 星期 + 月日 + 秒数，每秒更新 |
| 3 | 天气行 | 温度/风向/湿度/降水，每 10 秒随机变 |
| 4 | 中文模式 | 天气描述行变成农历日期（如"六月初二 大暑"） |
| 5 | 日出日落 | DAWN / DUSK 时间，每天更新 |
| 6 | 月相 | 顶部月相图标（或文字），每天更新 |
| 7 | 三字段 LED | RECOVERY/HR/ACT 数值，每 10 秒变 |
| 8 | 步数行 | 五位步数 + 左右状态图标 |
| 9 | 电池图标 | 底部电池图，每 10 秒变 |
| 10 | 串口日志 | `Segment34 Watchface starting...` → `timers created, done` |

### 5.2 真机验证（上机后）

| # | 检查项 | 方法 |
|---|--------|------|
| 1 | 屏幕亮起 | 上电后 ~1s 内亮屏 |
| 2 | 时间正确 | `time(NULL)` 取自系统；真机还未接 RTC，时间从 epoch 开始累计 |
| 3 | 串口输出 | `minicom -D /dev/ttyACM0 -b 115200` |
| 4 | 无花屏 | 若花屏先降 `mipi-max-frequency`（当前 32 MHz，改 overlay 后 `--pristine`） |

---

## 6. 常见问题排查

### 6.1 模拟器窗口打不开

1. `sudo apt-get install libsdl2-dev`
2. 确认 `boards/native_sim_native_64.conf` 有 `CONFIG_SDL_DISPLAY=y`
3. 确认**文件名**与 board 名匹配（`/` → `_`）：
   - `native_sim/native/64` → `boards/native_sim_native_64.conf`
   - 名字对不上会被**静默忽略**，不报错

### 6.2 窗口空白/透明

`main.c` 必须调 `display_blanking_off()`——Zephyr 显示默认 blanked。

### 6.3 中文显示成空心方框

先查 `CONFIG_LV_TXT_ENC_UTF8=y`（`prj.conf` + 板级 `.conf` 都要写），再查字体覆盖率
（见 FONTS.md §2.2）。

### 6.4 启动即段错误

LVGL OOM。开 `CONFIG_ASSERT=y` + `CONFIG_LV_USE_ASSERT_MALLOC=y` 复现；不要把
`CONFIG_LV_Z_MEM_POOL_SIZE` 调低于 64 KB（见 DEVELOPMENT.md §6.2）。

### 6.5 真机黑屏

1. 对照 §4 表格逐针核对接线（D 编号，不是 P0.xx）
2. 确认 `CONFIG_GC9X01X=y`（**不是** `GC9A01`——该驱动名在 Zephyr 里不存在）
3. 降 `mipi-max-frequency` 到 16 MHz 试试
4. 串口看有没有 `Segment34 Watchface starting...`

### 6.6 真机花屏/颜色异常

降 SPI 频率（overlay 里 `mipi-max-frequency`），改后必须 `--pristine` 重编。

### 6.7 编译找不到头文件/符号

```bash
west update
west build ... --pristine
```

### 6.8 数字显示 N-1

LED 字体手改遗漏，见 FONTS.md §5.3。

---

## 7. 目录结构速查

```
lgvl_watchUi/
├── README.md
├── gen_cjk_font.py             # CJK 字体生成器
├── app/
│   ├── CMakeLists.txt
│   ├── prj.conf
│   ├── Kconfig
│   ├── west.yml                # 非生效清单（见 PROJECT.md §1）
│   ├── boards/
│   │   ├── native_sim_native_64.conf/.overlay   # 模拟器
│   │   ├── xiao_ble_nrf52840_sense.conf/.overlay # 真机
│   │   └── zswatch.conf/.overlay                # ZSWatch（未验证）
│   ├── docs/
│   │   ├── PROJECT.md          # 架构总览（先看这个）
│   │   ├── DEVELOPMENT.md      # 开发细节
│   │   ├── HANDOVER.md         # 本文件
│   │   ├── FONTS.md            # 字体生成
│   │   └── LUNAR.md            # 农历数据
│   ├── scripts/
│   │   ├── bmfont2lvgl.py      # BMFont→LVGL（生成 segments80）
│   │   └── generate_font.py    # 旧 CJK 路线（已弃用）
│   └── src/
│       ├── main.c
│       ├── components/
│       │   ├── watchface.c/h   # 全部 UI 逻辑
│       │   └── lunar_calendar.c/h
│       ├── locale/locale.c/h
│       ├── theme/theme.c/h     # 8 套主题
│       └── fonts/              # 6 个自定义字体
└── tools/                      # 字体与农历表生成/校验脚本
```

---

## 8. 参考资源

| 资源 | URL |
|------|-----|
| Zephyr 文档 | https://docs.zephyrproject.org/ |
| LVGL 文档 | https://docs.lvgl.io/ |
| nRF52840 参考手册 | https://infocenter.nordicsemi.com/ |
| GC9A01 datasheet | https://www.newhavendisplay.com/specs/NHD-1.28-240240UCY3.pdf |
| 参考表盘（Segment34.CN） | https://github.com/laukeng/Segment34.CN |

---

**文档版本**：v2.0 · 2026-08-06  
**适用项目**：lgvl_watchUi（Zephyr v4.4.1 + LVGL v9.5.0）  
**目标硬件**：Seeed XIAO BLE nRF52840 Sense + GC9A01 240×240
