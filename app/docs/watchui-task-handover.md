# 任务交接：手表 UI 时钟背景纹理与浅色主题

## 项目背景

这是一个 Zephyr RTOS + LVGL 的智能手表 UI 项目，位于 WSL Ubuntu 环境：
- 项目路径：/home/zheng_fang/zephyr-project/lgvl_watchUi
- 关键文件：
  - app/src/theme/theme.h — 主题枚举和颜色结构体定义
  - app/src/theme/theme.c — 主题颜色数组实现
  - app/src/components/watchface.c — 手表表盘 UI 实现
- 操作环境说明：文件在 WSL 中，建议通过 wsl 命令或 bash -c 在 WSL 内用 sed/python 修改。

## 原始需求

1. 添加时钟背景网格纹理（使用 clock_off 颜色绘制网格线）
2. 添加三种浅色主题：yellow、pink、orange（浅色）
3. 删除旧的深色 orange 主题
4. 默认主题改为 yellow
5. 主题切换时同步更新时钟背景

## 当前进度

### 已完成（git commit 683061f）

theme.h 已修改：枚举添加了 THEME_YELLOW, THEME_PINK, THEME_ORANGE_LIGHT，删除了旧 THEME_ORANGE

### 未完成（存在严重问题）

theme.c 有严重问题：
- 只改了 current_theme = THEME_YELLOW
- themes[] 数组没有更新，顺序仍是 [green, blue, red, orange(旧深色), purple, cyan]
- 枚举值与数组索引不匹配，会导致主题颜色错乱！
- 文件头被误加了 BOM 字符
- 没有添加新的 yellow/pink/orange 浅色主题定义
- 没有删除旧的深色 orange 主题

watchface.c 完全未修改：
- 第126行 theme_init(THEME_GREEN) 需改为 THEME_YELLOW
- 第199行 clock_label = lv_label_create(root_page) 需重构为 clock_bg 容器
- 需新增 clock_draw_event_cb 绘制网格纹理函数
- 第449行 watchface_switch_theme 需添加 clock_bg 更新

## 需要完成的工作

### 任务 1：修复 theme.c

1a. 移除文件头 BOM 字符

1b. 在 green 主题之前添加三个浅色主题定义（数组开头），颜色值：

yellow: name=yellow, bg=0x080c14, clock_on=0xffe880, clock_off=0x1a1508, text=0xffe880, accent=0xffaa00, weather=0x88ccff, heart_rate=0xff6666, steps=0x88ffcc, battery=0xffe880, field_lbl=0x88aaff, field_bg=0x0a2040, data_val=0xffe880, stress=0xff66aa, bodybatt=0x88ffff, notif=0xff88ff, moon=0xffff88, outline=0x4466aa

pink: name=pink, bg=0x0c0814, clock_on=0xff99cc, clock_off=0x1a0a15, text=0xff99cc, accent=0xffaa00, weather=0x88ccff, heart_rate=0xff6666, steps=0x88ffcc, battery=0xff99cc, field_lbl=0x88aaff, field_bg=0x0a2040, data_val=0xff99cc, stress=0xff66aa, bodybatt=0x88ffff, notif=0xff88ff, moon=0xffff88, outline=0x4466aa

orange(浅色): name=orange, bg=0x0c0a08, clock_on=0xffb878, clock_off=0x1a1208, text=0xffb878, accent=0xffaa00, weather=0x88ccff, heart_rate=0xff6666, steps=0x88ffcc, battery=0xffb878, field_lbl=0x88aaff, field_bg=0x0a2040, data_val=0xffb878, stress=0xff66aa, bodybatt=0x88ffff, notif=0xff88ff, moon=0xffff88, outline=0x4466aa

1c. 删除旧的深色 orange 主题（在 red 和 purple 之间，特征是 .clock_on=0xffaa00）

1d. 确保数组顺序与枚举一致：[yellow, pink, orange, green, blue, red, purple, cyan]

### 任务 2：修改 watchface.c

2a. 第126行：theme_init(THEME_GREEN) 改为 theme_init(THEME_YELLOW)

2b. 添加 clock_draw_event_cb 函数（在静态变量声明之后），绘制8像素网格纹理，使用 colors->clock_off 颜色，透明度 LV_OPA_20。函数原型：
static void clock_draw_event_cb(lv_event_t *e)
内部使用 lv_draw_rect 绘制网格线，遍历 x 和 y 方向，步长8像素。

2c. 创建 clock_bg 容器（lv_obj_create），设置位置(CLOCK_X, CLOCK_Y)和大小(CLOCK_W, CLOCK_H)，黑色背景，注册 LV_EVENT_DRAW_MAIN 回调，将 clock_label 作为 clock_bg 子对象。

2d. 在 watchface_switch_theme 中添加 lv_obj_invalidate(clock_bg) 和 date_label 颜色改为 colors->clock_on。

### 任务 3：验证与提交

1. 检查 theme.c 数组顺序与 theme.h 枚举顺序一致
2. 检查无 BOM 字符
3. git 提交

## 关键注意事项

1. 操作 WSL 文件必须通过 WSL 内部命令（sed/python/bash）
2. theme.c 枚举与数组索引必须严格对应，当前最大的 bug 就是两者不匹配
3. 旧的深色 orange 主题特征：clock_on=0xffaa00，新的浅色 orange：clock_on=0xffb878
