#ifndef WATCHFACE_H
#define WATCHFACE_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void watchface_start(void);
void watchface_stop(void);
void watchface_update_time(void);
void watchface_update_date(void);
void watchface_update_weather(void);
void watchface_update_battery(void);
void watchface_update_sensors(void);
void watchface_switch_language(void);
void watchface_switch_theme(void);
void watchface_switch_battery_display(void);

#ifdef __cplusplus
}
#endif

#endif