#include "locale.h"

language_t current_lang = LANG_ZH;

static const char *zh_strings[] = {
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x80",  /* 星期一 */
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x8c",  /* 星期二 */
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x89",  /* 星期三 */
    "\xe6\x98\x9f\xe6\x9c\x9f\xe5\x9b\x9b",  /* 星期四 */
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x94",  /* 星期五 */
    "\xe6\x98\x9f\xe6\x9c\x9f\xe5\x85\xad",  /* 星期六 */
    "\xe6\x98\x9f\xe6\x9c\x9f\xe6\x97\xa5",  /* 星期日 */
    "\xe4\xb8\x80\xe6\x9c\x88",              /* 一月 */
    "\xe4\xba\x8c\xe6\x9c\x88",              /* 二月 */
    "\xe4\xb8\x89\xe6\x9c\x88",              /* 三月 */
    "\xe5\x9b\x9b\xe6\x9c\x88",              /* 四月 */
    "\xe4\xba\x94\xe6\x9c\x88",              /* 五月 */
    "\xe5\x85\xad\xe6\x9c\x88",              /* 六月 */
    "\xe4\xb8\x83\xe6\x9c\x88",              /* 七月 */
    "\xe5\x85\xab\xe6\x9c\x88",              /* 八月 */
    "\xe4\xb9\x9d\xe6\x9c\x88",              /* 九月 */
    "\xe5\x8d\x81\xe6\x9c\x88",              /* 十月 */
    "\xe5\x8d\x81\xe4\xb8\x80\xe6\x9c\x88",  /* 十一月 */
    "\xe5\x8d\x81\xe4\xba\x8c\xe6\x9c\x88",  /* 十二月 */
    "\xe6\x99\xb4",                          /* 晴 */
    "\xe5\xa4\x9a\xe4\xba\x91",              /* 多云 */
    "\xe9\x98\xb4",                          /* 阴 */
    "\xe9\x9b\xa8",                          /* 雨 */
    "\xe9\x9b\xaa",                          /* 雪 */
    "\xe5\xbf\x83\xe7\x8e\x87",              /* 心率 */
    "\xe6\xad\xa5\xe6\x95\xb0",              /* 步数 */
    "\xe7\x94\xb5\xe6\xb1\xa0",              /* 电池 */
    "\xe6\xa5\xbc\xe5\xb1\x82",              /* 楼层 */
    "\xe5\x8d\xa1\xe8\xb7\xaf\xe9\x87\x8c",  /* 卡路里 */
    "\xe6\x97\xa5\xe5\x87\xba",              /* 日出 */
    "\xe6\x97\xa5\xe8\x90\xbd",              /* 日落 */
    "\xe6\x96\xb0\xe6\x9c\x88",              /* 新月 */
    "\xe4\xb8\x8a\xe5\xbc\xa6",              /* 上弦 */
    "\xe6\xbb\xa1\xe6\x9c\x88",              /* 满月 */
    "\xe4\xb8\x8b\xe5\xbc\xa6",              /* 下弦 */
    "\xe5\xa4\x9a\xe4\xba\x91",              /* 多云 (PARTLY CLOUDY) */
    "\xe4\xb8\xad\xe6\x96\x87",              /* 中文 */
    "English",
};

static const char *en_strings[] = {
    "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN",
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC",
    "SUNNY", "CLOUDY", "OVERCAST", "RAIN", "SNOW",
    "HR", "STEPS", "BATTERY",
    "FLOORS", "CAL",
    "DAWN", "DUSK",
    "NEW", "1QTR", "FULL", "3QTR",
    "PARTLY CLOUDY",
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
