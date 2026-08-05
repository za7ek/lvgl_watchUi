#ifndef LUNAR_CALENDAR_H
#define LUNAR_CALENDAR_H

#include <stdint.h>

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t leap_month;
    uint8_t day_of_week;
    char year_name[20];
    char month_name[16];
    char day_name[8];
    char jieqi[16];
} lunar_date_t;

void lunar_calendar_convert(uint16_t solar_year, uint8_t solar_month, uint8_t solar_day, lunar_date_t *lunar);

#endif