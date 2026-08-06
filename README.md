# lgvl_watchUi — Segment34 风格表盘（Zephyr + LVGL）

一个跑在 Zephyr RTOS + LVGL 上的智能手表表盘，视觉与数据布局对齐 Garmin Connect IQ
表盘 [Segment34.CN](https://github.com/laukeng/Segment34.CN)：段码大时钟、LED 点阵数值、
日出日落 / 月相 / 天气 / 农历 / 状态图标，中英双语、8 套主题。

目标硬件是 Seeed XIAO BLE (nRF52840) + GC9A01 1.28" 圆屏 240×240；开发时在
`native_sim`（SDL）里跑同分辨率的模拟器。

## 一分钟上手

```bash
cd ~/zephyr-project && source .venv/bin/activate

# 模拟器：编译 + 运行
west build -b native_sim/native/64 -d ~/zephyr-project/native_ui ~/zephyr-project/lgvl_watchUi/app
west build -t run -d ~/zephyr-project/native_ui

# 真机：编译 + 烧录
west build -b xiao_ble/nrf52840/sense -d ~/zephyr-project/xiao_build ~/zephyr-project/lgvl_watchUi/app
west flash -d ~/zephyr-project/xiao_build
```

## 文档索引

| 文档 | 讲什么 | 什么时候看 |
|------|--------|-----------|
| [app/docs/PROJECT.md](app/docs/PROJECT.md) | 架构总览：目录结构、每个模块干什么、屏幕布局坐标、渲染分层、常见修改点 | **先看这个** |
| [app/docs/DEVELOPMENT.md](app/docs/DEVELOPMENT.md) | 开发细节：配置项、内存预算、圆形可视区校核、重绘抑制、调试手法 | 动代码之前 |
| [app/docs/HANDOVER.md](app/docs/HANDOVER.md) | 交接：环境准备、接线、烧录、验证清单、故障排查 | 换人接手 / 上真机 |
| [app/docs/FONTS.md](app/docs/FONTS.md) | 6 个自定义字体的来源、生成脚本、必做手改、踩过的坑 | 改字体 / 加字符 |
| [app/docs/LUNAR.md](app/docs/LUNAR.md) | 农历与节气查表数据的生成和校验（2026-2056） | 扩年份 / 查农历不对 |

## 现状速记

- Zephyr **v4.4.1**（workspace 实际版本）+ LVGL **v9.5.0**
- 传感器数据全是 `watchface.c` 里的模拟值，尚未接真实驱动
- 时间取自 `time(NULL)`，真机上还没有接 RTC
- 语言/主题/月相/电池显示的切换函数已就绪，但**还没有接输入事件**，目前只能从代码里调用
