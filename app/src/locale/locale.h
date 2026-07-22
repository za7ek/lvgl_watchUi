#ifndef LOCALE_H
#define LOCALE_H

#include <stdint.h>

typedef enum {
    LANG_ZH,
    LANG_EN,
    LANG_COUNT
} language_t;

typedef enum {
    LOCALE_STR_MONDAY,
    LOCALE_STR_TUESDAY,
    LOCALE_STR_WEDNESDAY,
    LOCALE_STR_THURSDAY,
    LOCALE_STR_FRIDAY,
    LOCALE_STR_SATURDAY,
    LOCALE_STR_SUNDAY,
    
    LOCALE_STR_JANUARY,
    LOCALE_STR_FEBRUARY,
    LOCALE_STR_MARCH,
    LOCALE_STR_APRIL,
    LOCALE_STR_MAY,
    LOCALE_STR_JUNE,
    LOCALE_STR_JULY,
    LOCALE_STR_AUGUST,
    LOCALE_STR_SEPTEMBER,
    LOCALE_STR_OCTOBER,
    LOCALE_STR_NOVEMBER,
    LOCALE_STR_DECEMBER,
    
    LOCALE_STR_CLEAR,
    LOCALE_STR_CLOUDY,
    LOCALE_STR_OVERCAST,
    LOCALE_STR_RAIN,
    LOCALE_STR_SNOW,
    
    LOCALE_STR_HEART_RATE,
    LOCALE_STR_STEPS,
    LOCALE_STR_BATTERY,
    
    LOCALE_STR_CHINESE,
    LOCALE_STR_ENGLISH,
    
    LOCALE_STR_COUNT
} locale_str_id_t;

extern language_t current_lang;

void locale_set_current(language_t lang);
language_t locale_get_current(void);
const char *locale_get_string(locale_str_id_t id);

#endif