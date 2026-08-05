#ifndef WATCHFACE_H
#define WATCHFACE_H

#include <lvgl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 倒数第二行（步数点阵）左右各有一个状态图标位。每个位置选一种指示器，
 * 具体画哪个字形——或者什么都不画——由下面那几个状态量决定。
 * 同 Segment34 的 icon1/icon2 设置项和 getIconState()。 */
typedef enum {
    ICON_SLOT_NONE = 0,      /* 该位置不显示 */
    ICON_SLOT_ALARM,         /* 有闹钟时显示闹钟，否则空 */
    ICON_SLOT_DND,           /* 勿扰开启时显示，否则空 */
    ICON_SLOT_BLUETOOTH,     /* 连接/断开各有图标，始终显示 */
    ICON_SLOT_BLUETOOTH_OFF, /* 只在断开时显示，连上就空 */
    ICON_SLOT_MOVE_BAR,      /* 久坐提醒 1-5 级，0 级空 */
} icon_slot_t;

void watchface_start(void);
void watchface_stop(void);
void watchface_update_time(void);
void watchface_update_date(void);
void watchface_update_weather(void);
void watchface_update_battery(void);
void watchface_update_sensors(void);
void watchface_update_icons(void);
void watchface_switch_language(void);
void watchface_switch_theme(void);
void watchface_switch_battery_display(void);
void watchface_switch_moon_display(void);

/* 两个图标位分别显示哪种指示器 */
void watchface_set_icon_slots(icon_slot_t left, icon_slot_t right);

/* 状态输入。目前由 sim_update_data() 喂模拟值；接真实数据源时（RTC 闹钟、
 * BLE 连接状态、活动监测）改成从那边调这几个函数即可，渲染侧不用动。 */
void watchface_set_alarm_count(int count);
void watchface_set_dnd(bool on);
void watchface_set_phone_connected(bool connected);
void watchface_set_move_bar_level(int level);   /* 0-5，0 表示不提醒 */

#ifdef __cplusplus
}
#endif

#endif