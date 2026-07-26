# LVGL Watch UI 交接使用文档

## 1. 项目概述

### 1.1 项目定位

本项目是一个基于 LVGL 的智能手表 UI 应用，运行在 Seeed XIAO BLE (nRF52840) 开发板上，搭配 1.28 英寸圆形 GC9A01 显示屏。

### 1.2 主要功能

- **Segment34 数码时钟**：34段数码管风格的时间显示
- **日期显示**：公历日期 + 农历日期
- **传感器数据**：心率、步数、电量模拟显示
- **天气信息**：当前天气模拟显示
- **多语言支持**：中文/英文切换
- **多主题支持**：6种配色方案

### 1.3 硬件清单

| 组件 | 型号 | 数量 |
|------|------|------|
| 开发板 | Seeed XIAO BLE (nRF52840 Sense) | 1 |
| 显示屏 | GC9A01 1.28英寸圆形 240x240 | 1 |
| 连接线 | 杜邦线 | 若干 |
| USB-C 线 | USB-C to USB-A | 1 |

---

## 2. 快速上手

### 2.1 环境要求

- **操作系统**: Ubuntu 20.04+ (WSL2)
- **Python**: 3.8+
- **Zephyr SDK**: v0.16+
- **CMake**: 3.20+
- **SDL2**: 用于模拟器显示

### 2.2 环境初始化

```bash
# 进入项目目录
cd ~/zephyr-project

# 激活虚拟环境
source .venv/bin/activate

# 检查环境
west --version
cmake --version
```

### 2.3 编译模拟器版本

```bash
# 编译
west build -b native_sim lgvl_watchUi/app --build-dir native_ui

# 运行
cd ~/zephyr-project/native_ui
./zephyr.exe
```
```bash
# 编译
west build -b native_sim/native/64 -d ~/zephyr-project/native_ui  ~/zephyr-project/lgvl_watchUi/app/
# 运行
west build -t run -d ~/zephyr-project/native_ui 
```

### 2.4 编译真机版本

```bash
# 编译
west build -b xiao_ble lgvl_watchUi/app

# 烧录
west flash
```

---

## 3. 编译与部署流程

### 3.1 编译流程

```
源代码 (src/)
    ↓
West Build (CMake + Ninja)
    ↓
Zephyr RTOS 内核
    ↓
LVGL 图形库
    ↓
应用代码链接
    ↓
可执行文件 (zephyr.elf/zephyr.exe)
```

### 3.2 编译产物说明

#### 模拟器版本 (native_sim)

| 文件 | 路径 | 用途 |
|------|------|------|
| zephyr.exe | native_ui/zephyr/zephyr.exe | Windows 可执行文件 |
| zephyr.elf | native_ui/zephyr/zephyr.elf | ELF 格式文件 |
| zephyr.map | native_ui/zephyr/zephyr.map | 链接映射文件 |

#### 真机版本 (xiao_ble)

| 文件 | 路径 | 用途 |
|------|------|------|
| zephyr.hex | build/zephyr/zephyr.hex | Intel HEX 格式，用于烧录 |
| zephyr.elf | build/zephyr/zephyr.elf | ELF 格式，用于调试 |
| zephyr.bin | build/zephyr/zephyr.bin | 二进制格式 |

### 3.3 烧录流程

```bash
# 方式一：使用 west flash（推荐）
west flash

# 方式二：使用 nrfjprog
nrfjprog --program build/zephyr/zephyr.hex --chiperase --reset

# 方式三：使用 J-Link
JLinkExe -device nRF52840_xxAA -if SWD -speed 4000
> loadfile build/zephyr/zephyr.hex
> r
> q
```

### 3.4 串口调试

```bash
# 查找串口设备
ls /dev/ttyACM*

# 打开串口终端（波特率 115200）
minicom -D /dev/ttyACM0 -b 115200

# 或使用 screen
screen /dev/ttyACM0 115200
```

---

## 4. 硬件接线指南

### 4.1 GC9A01 显示屏接线

| GC9A01 引脚 | XIAO BLE 引脚 | 功能 |
|-------------|---------------|------|
| VCC | 3V3 | 电源 (3.3V) |
| GND | GND | 地线 |
| DIN | P0.10 | SPI MOSI |
| CLK | P0.09 | SPI SCK |
| CS | P0.11 | 片选 (低电平有效) |
| DC | P0.13 | 数据/命令选择 |
| RST | P0.14 | 复位 (低电平有效) |
| BL | P0.12 | 背光控制 (高电平开启) |

### 4.2 接线示意图

```
GC9A01                    XIAO BLE
┌─────────┐              ┌─────────────┐
│  VCC    │──────────────│ 3V3         │
│  GND    │──────────────│ GND         │
│  DIN    │──────────────│ P0.10 (MOSI)│
│  CLK    │──────────────│ P0.09 (SCK) │
│  CS     │──────────────│ P0.11       │
│  DC     │──────────────│ P0.13       │
│  RST    │──────────────│ P0.14       │
│  BL     │──────────────│ P0.12       │
└─────────┘              └─────────────┘
```

### 4.3 注意事项

1. **电源稳定性**：确保 3.3V 电源能够提供足够电流（建议 > 100mA）
2. **SPI 速度**：GC9A01 支持最高 40MHz SPI 时钟
3. **背光控制**：BL 引脚接 PWM 可实现亮度调节
4. **静电防护**：操作时注意防静电

---

## 5. 功能验证清单

### 5.1 基础功能验证

| 功能 | 验证方法 | 预期结果 |
|------|----------|----------|
| 时间显示 | 观察数码时钟 | 显示当前时间，秒冒号闪烁 |
| 日期显示 | 观察日期标签 | 显示公历日期和星期 |
| 农历显示 | 观察农历标签 | 显示农历日期（如：正月十五） |
| 天气显示 | 观察天气标签 | 显示模拟天气信息 |
| 心率显示 | 观察心率标签 | 显示模拟心率数据 |
| 步数显示 | 观察步数标签 | 显示模拟步数数据 |
| 电量显示 | 观察电量标签和进度条 | 显示电量百分比和进度条 |

### 5.2 交互功能验证

| 功能 | 验证方法 | 预期结果 |
|------|----------|----------|
| 语言切换 | 调用 `watchface_switch_language()` | 界面文字在中英文之间切换 |
| 主题切换 | 调用 `watchface_switch_theme()` | 界面颜色在6种主题之间切换 |
| 时间更新 | 等待1分钟 | 时间自动更新，冒号每秒闪烁 |
| 传感器更新 | 等待10秒 | 传感器数据自动刷新 |

### 5.3 性能验证

| 指标 | 验证方法 | 预期结果 |
|------|----------|----------|
| 帧率 | 观察 UI 流畅度 | > 20 FPS |
| 内存占用 | 查看日志或调试 | < 60KB |
| Flash 占用 | 查看 map 文件 | < 500KB |

---

## 6. 常见问题与解决方案

### 6.1 编译问题

#### Q: 找不到 LVGL 头文件

```
fatal error: lvgl.h: No such file or directory
```

**解决方案**：
```bash
west update
```

确保 `west.yml` 中的 LVGL 模块已正确下载。

#### Q: CMake 配置失败

```
CMake Error: Could not find Zephyr.
```

**解决方案**：
```bash
source .venv/bin/activate
export ZEPHYR_BASE=~/zephyr-project/zephyr
```

#### Q: SDL 相关编译错误

```
error: 'SDL_Window' has not been declared
```

**解决方案**：
```bash
sudo apt-get install libsdl2-dev
```

### 6.2 运行问题

#### Q: 模拟器窗口无法打开

**现象**：运行 `zephyr.exe` 后无窗口显示

**解决方案**：
1. 检查 SDL2 是否已安装
2. 确保 `native_sim.conf` 中 `CONFIG_SDL_DISPLAY=y`
3. 检查系统图形驱动

#### Q: 真机屏幕无显示

**现象**：烧录后屏幕黑屏或显示异常

**排查步骤**：

1. **检查接线**：对照接线图逐一核对
2. **检查电源**：测量 VCC 引脚电压（应为 3.3V）
3. **检查复位**：手动复位开发板
4. **检查日志**：通过串口查看启动日志

```bash
# 串口日志示例（正常启动）
[00:00:00.000,000] <inf> main: Segment34 Watchface starting...
```

#### Q: 屏幕显示花屏或颜色异常

**现象**：屏幕显示内容混乱或颜色不对

**解决方案**：
1. 降低 SPI 频率（在 `xiao_ble.overlay` 中修改）
2. 检查 DC/CS/RST 引脚接线
3. 确认 GC9A01 驱动配置正确

#### Q: 时间显示错误

**现象**：显示的时间与实际时间不符

**解决方案**：
1. 检查系统时钟配置
2. 确认 `CONFIG_CLOCK_CONTROL_NRF=y`
3. 检查 RTC 初始化

### 6.3 性能问题

#### Q: UI 卡顿

**现象**：界面刷新不流畅

**解决方案**：
1. 减少 VDB 大小（`CONFIG_LV_Z_VDB_SIZE`）
2. 减少 LVGL 堆大小（`CONFIG_LV_Z_MEM_POOL_SIZE`）
3. 优化动画效果

#### Q: 内存不足

**现象**：运行时崩溃或日志显示内存错误

**解决方案**：
1. 减少同时显示的组件数量
2. 优化 Segment34 组件（已优化）
3. 启用内存统计：`CONFIG_MEM_STATS=y`

---

## 7. 配置文件说明

### 7.1 配置文件清单

| 文件 | 位置 | 用途 |
|------|------|------|
| prj.conf | app/prj.conf | 默认配置（通用） |
| native_sim.conf | app/boards/native_sim.conf | 模拟器配置 |
| native_sim.overlay | app/boards/native_sim.overlay | 模拟器设备树 |
| xiao_ble.conf | app/boards/xiao_ble.conf | 真机配置 |
| xiao_ble.overlay | app/boards/xiao_ble.overlay | 真机设备树 |

### 7.2 关键配置项

#### LVGL 配置

```
CONFIG_LVGL=y                          # 启用 LVGL
CONFIG_LV_COLOR_DEPTH_16=y             # 16位色深
CONFIG_LV_Z_MEM_POOL_SIZE=32768        # LVGL 堆大小
CONFIG_LV_Z_VDB_SIZE=10                # VDB 大小百分比
```

#### 显示配置

```
# 模拟器
CONFIG_SDL_DISPLAY=y

# 真机
CONFIG_DISPLAY=y
CONFIG_GC9A01=y
CONFIG_SPI=y
```

#### 应用配置

```
CONFIG_SEGMENT34_SHOW_LUNAR=y          # 显示农历
CONFIG_SEGMENT34_LANGUAGE="zh"         # 默认语言
CONFIG_SEGMENT34_THEME="green"         # 默认主题
```

---

## 8. 目录结构速查

```
lgvl_watchUi/app/
├── CMakeLists.txt                     # CMake 配置
├── Kconfig                            # 配置选项定义
├── prj.conf                           # 默认配置
├── west.yml                           # West 清单
├── boards/                            # 板级配置
│   ├── native_sim.conf/overlay        # 模拟器配置
│   └── xiao_ble.conf/overlay          # 真机配置
├── src/                               # 源代码
│   ├── main.c                         # 入口文件
│   ├── components/                    # UI 组件
│   │   ├── segment34.c/h              # 数码时钟
│   │   ├── watchface.c/h              # 表盘主组件
│   │   └── lunar_calendar.c/h         # 农历转换
│   ├── fonts/                         # 字体文件
│   ├── locale/                        # 多语言支持
│   └── theme/                         # 主题系统
├── scripts/                           # 辅助脚本
│   └── generate_font.py               # 字体生成
└── docs/                              # 文档
    ├── DEVELOPMENT.md                 # 开发文档
    └── HANDOVER.md                    # 交接文档（本文档）
```

---

## 9. 开发资源

### 9.1 参考文档

| 资源 | URL |
|------|-----|
| Zephyr RTOS 文档 | https://docs.zephyrproject.org/ |
| LVGL 文档 | https://docs.lvgl.io/ |
| nRF52840 参考手册 | https://infocenter.nordicsemi.com/ |
| GC9A01 数据手册 | https://www.newhavendisplay.com/specs/NHD-1.28-240240UCY3.pdf |

### 9.2 工具链

| 工具 | 用途 | 安装命令 |
|------|------|----------|
| West | Zephyr 构建工具 | pip install west |
| nrfjprog | nRF 烧录工具 | 从 Nordic 官网下载 |
| minicom | 串口终端 | sudo apt install minicom |
| lv_font_conv | LVGL 字体生成 | npm install -g lv_font_conv |

### 9.3 常用命令

```bash
# 清理构建
west build -t clean

# 重新配置
west build -b <board> <app> --pristine

# 查看配置
west build -t menuconfig

# 查看构建信息
west build -t build_info

# 运行单元测试
west test
```

---

## 10. 维护建议

### 10.1 日常维护

1. **定期更新依赖**：`west update`
2. **代码审查**：提交前进行代码审查
3. **测试验证**：编译前后进行模拟器和真机测试

### 10.2 版本管理

1. 使用 Git 进行版本控制
2. 遵循语义化版本规范
3. 重要修改添加版本说明

### 10.3 故障处理流程

```
发现问题
    ↓
记录现象（日志、截图）
    ↓
定位原因（代码分析、调试）
    ↓
实施修复
    ↓
验证修复（模拟器 + 真机）
    ↓
文档更新
```

---

## 附录：常见错误代码

| 错误代码 | 含义 | 解决方案 |
|----------|------|----------|
| -1 | 编译失败 | 检查编译日志，修复错误 |
| 0 | 成功 | 正常 |
| 1 | 参数错误 | 检查命令参数 |
| 2 | 文件不存在 | 检查文件路径 |
| 127 | 命令未找到 | 检查工具是否安装 |

---

**文档版本**: v1.0  
**创建日期**: 2026-07-23  
**适用项目**: lgvl_watchUi  
**目标硬件**: Seeed XIAO BLE + GC9A01