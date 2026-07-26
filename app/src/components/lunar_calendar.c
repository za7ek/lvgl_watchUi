#include <stdio.h>
#include "lunar_calendar.h"

static const uint32_t lunar_year_data[] = {
    0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0,
    0x09ad0, 0x055d2, 0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540,
    0x0d6a0, 0x0ada2, 0x095b0, 0x14977, 0x04970, 0x0a4b0, 0x0b4b5, 0x06a50,
    0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970, 0x06566, 0x0d4a0,
    0x0ea50, 0x06e95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950,
    0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2,
    0x0a950, 0x0b557, 0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5d0, 0x04572,
    0x0d2d0, 0x0d2d5, 0x0b5a0, 0x056d0, 0x04dd0, 0x0a4d0, 0x1d4d7, 0x0d2d0,
    0x0d5a0, 0x05d55, 0x05ba0, 0x06d50, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250,
    0x0d558, 0x0b540, 0x0b5a0, 0x195a6, 0x095b0, 0x049b0, 0x0a974, 0x0a4b0,
    0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570, 0x04af5, 0x04970,
    0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x055c0, 0x0ab60, 0x096d5, 0x092e0,
    0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0,
    0x092d0, 0x0cab5, 0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0,
    0x0a5b0, 0x15176, 0x052b0, 0x0a9a0, 0x0b9a4, 0x06aa0, 0x0aea0, 0x05b55,
    0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530, 0x05aa0, 0x076a3,
    0x096d0, 0x04bd7, 0x04ad0, 0x0a4d0, 0x0d0b6, 0x0d250, 0x0d520, 0x0dd45,
    0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255,
    0x06d20, 0x0ada0
};

static const char *gan[] = {"甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸"};
static const char *zhi[] = {"子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥"};
static const char *shengxiao[] = {"鼠", "牛", "虎", "兔", "龙", "蛇", "马", "羊", "猴", "鸡", "狗", "猪"};

static const char *month_names[] = {
    "", "正", "二", "三", "四", "五", "六", "七", "八", "九", "十", "冬", "腊"
};

static const char *day_names[] = {
    "", "初", "十", "廿", "三"
};

static const char *day_digits[] = {
    "", "一", "二", "三", "四", "五", "六", "七", "八", "九"
};

void lunar_calendar_convert(uint16_t solar_year, uint8_t solar_month, uint8_t solar_day, lunar_date_t *lunar)
{
    uint32_t base_date = (solar_year - 1900) * 365 
                      + (solar_year - 1900 + 3) / 4 
                      + solar_day - 1;
    
    for (int i = 1; i < solar_month; i++) {
        uint32_t days_in_month;
        if (i == 2) {
            days_in_month = ((solar_year % 4 == 0 && solar_year % 100 != 0) || solar_year % 400 == 0) ? 29 : 28;
        } else if (i == 4 || i == 6 || i == 9 || i == 11) {
            days_in_month = 30;
        } else {
            days_in_month = 31;
        }
        base_date += days_in_month;
    }
    
    int32_t offset = base_date - 2392432;
    
    uint16_t lYear = 1900;
    while (offset > 0) {
        uint32_t year_code = lunar_year_data[lYear - 1900];
        uint32_t days_in_lunar_year = 0;
        
        uint8_t leap_month = year_code & 0x0F;
        
        for (int i = 0; i < 12; i++) {
            uint8_t bit_pos = 4 + i * 2;
            uint8_t month_days = ((year_code >> bit_pos) & 0x03);
            days_in_lunar_year += (month_days == 0) ? 29 : 30;
        }
        
        if (leap_month != 0) {
            uint8_t bit_pos = 4 + (leap_month - 1) * 2;
            uint8_t leap_days = ((year_code >> bit_pos) & 0x03);
            days_in_lunar_year += (leap_days == 0) ? 29 : 30;
        }
        
        if (offset >= (int32_t)days_in_lunar_year) {
            offset -= days_in_lunar_year;
            lYear++;
        } else {
            break;
        }
    }
    
    uint32_t year_code = lunar_year_data[lYear - 1900];
    uint8_t leap_month = year_code & 0x0F;
    uint8_t lMonth = 1;
    uint8_t lDay = 1;
    uint8_t is_leap = 0;
    
    while (offset > 0) {
        uint8_t current_month = (is_leap) ? leap_month : lMonth;
        uint8_t bit_pos = 4 + (current_month - 1) * 2;
        uint8_t month_days_code = ((year_code >> bit_pos) & 0x03);
        uint32_t days_in_month = (month_days_code == 0) ? 29 : 30;
        
        if (offset >= (int32_t)days_in_month) {
            offset -= days_in_month;
            
            if (is_leap) {
                is_leap = 0;
                lMonth++;
            } else {
                if (lMonth == leap_month) {
                    is_leap = 1;
                } else {
                    lMonth++;
                }
            }
        } else {
            break;
        }
    }
    
    lDay = offset + 1;
    
    lunar->year = lYear;
    lunar->month = lMonth;
    lunar->day = lDay;
    lunar->leap_month = is_leap;
    lunar->day_of_week = (base_date + 1) % 7;
    
    snprintf(lunar->year_name, sizeof(lunar->year_name), "%s%s年(%s)", 
             gan[(lYear - 1900) % 10], zhi[(lYear - 1900) % 12], shengxiao[(lYear - 1900) % 12]);
    snprintf(lunar->month_name, sizeof(lunar->month_name), "%s月", month_names[lMonth]);
    
    if (lDay <= 10) {
        snprintf(lunar->day_name, sizeof(lunar->day_name), "%s%s", day_names[1], day_digits[lDay]);
    } else if (lDay < 20) {
        snprintf(lunar->day_name, sizeof(lunar->day_name), "%s%s", day_names[2], day_digits[lDay - 10]);
    } else if (lDay == 20) {
        snprintf(lunar->day_name, sizeof(lunar->day_name), "%s十", day_names[2]);
    } else if (lDay < 30) {
        snprintf(lunar->day_name, sizeof(lunar->day_name), "%s%s", day_names[3], day_digits[lDay - 20]);
    } else {
        snprintf(lunar->day_name, sizeof(lunar->day_name), "%s十", day_names[4]);
    }
    
    snprintf(lunar->jieqi, sizeof(lunar->jieqi), "");
}

const char* lunar_get_year_name(uint16_t year)
{
    static char buf[20];
    snprintf(buf, sizeof(buf), "%s%s年(%s)", 
             gan[(year - 1900) % 10], zhi[(year - 1900) % 12], shengxiao[(year - 1900) % 12]);
    return buf;
}

const char* lunar_get_month_name(uint8_t month, uint8_t leap)
{
    static char buf[16];
    if (leap) {
        snprintf(buf, sizeof(buf), "闰%s月", month_names[month]);
    } else {
        snprintf(buf, sizeof(buf), "%s月", month_names[month]);
    }
    return buf;
}

const char* lunar_get_day_name(uint8_t day)
{
    static char buf[8];
    if (day <= 10) {
        snprintf(buf, sizeof(buf), "%s%s", day_names[1], day_digits[day]);
    } else if (day < 20) {
        snprintf(buf, sizeof(buf), "%s%s", day_names[2], day_digits[day - 10]);
    } else if (day == 20) {
        snprintf(buf, sizeof(buf), "%s十", day_names[2]);
    } else if (day < 30) {
        snprintf(buf, sizeof(buf), "%s%s", day_names[3], day_digits[day - 20]);
    } else {
        snprintf(buf, sizeof(buf), "%s十", day_names[4]);
    }
    return buf;
}

const char* lunar_get_jieqi(uint16_t year, uint8_t month, uint8_t day)
{
    (void)year;
    (void)month;
    (void)day;
    return "";
}