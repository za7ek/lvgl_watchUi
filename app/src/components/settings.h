#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * settings — 运行时设置面板（LVGL overlay）
 *
 * 用法：
 *   1. watchface_start() 末尾调用 settings_init() 一次
 *   2. 长按表盘（或按硬件按钮）调用 settings_show() 弹出面板
 *   3. 面板内点击对应按钮即可切换语言/主题/月相/电池显示
 *   4. 点击面板外区域或 3 秒无操作自动关闭
 */

void settings_init(void);           /* 创建面板对象，只调一次 */
void settings_show(void);           /* 弹出设置面板 */
void settings_hide(void);           /* 关闭设置面板 */
bool settings_is_visible(void);     /* 面板当前是否可见 */

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_H */
