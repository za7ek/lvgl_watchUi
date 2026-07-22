#include "locale.h"

language_t current_lang = LANG_ZH;

static const char *zh_strings[] = {
    "星期一", "星期二", "星期三", "星期四", "星期五", "星期六", "星期日",
    "一月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月", "十一月", "十二月",
    "晴", "多云", "阴", "雨", "雪",
    "心率", "步数", "电池",
    "中文", "English",
};

static const char *en_strings[] = {
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday",
    "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December",
    "Sunny", "Cloudy", "Overcast", "Rain", "Snow",
    "HR", "Steps", "Battery",
    "Chinese", "English",
};

void locale_set_current(language_t lang)
{
    current_lang = lang;
}

language_t locale_get_current(void)
{
    return current_lang;
}

const char *locale_get_string(locale_str_id_t id)
{
    if (id >= LOCALE_STR_COUNT) {
        return "";
    }
    
    if (current_lang == LANG_ZH) {
        return zh_strings[id];
    } else {
        return en_strings[id];
    }
}