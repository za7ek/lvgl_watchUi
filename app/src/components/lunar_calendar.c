/*
 * Lunar calendar conversion and solar terms (节气).
 * Ported from Segment34.CN/source/LunarCalendar.mc (Garmin Monkey C reference),
 * extended to 2026-2056 via tools/gen_lunar_tables.py.
 *
 * Algorithm: lookup-table based.
 *   - LUNAR_OFFSET_DAYS[i] = day offset from START_YEAR-01-01 to the start of the i-th lunar month.
 *   - SOLAR_TERMS_OFFSETS[i] = day offset from START_YEAR-01-01 to the i-th solar term.
 *   - LEAP_MONTH_OFFSETS = indices in LUNAR_OFFSET_DAYS that are leap months.
 *
 * Supported year range: 2026-2056.
 * Lunar month starts and leap months are computed from astronomical new-moon
 * times (ephem); solar terms are computed from the Sun's ecliptic longitude
 * crossing each 15° mark. Both are verified against the Segment34.CN
 * reference data for 2026-2030.
 * Outside this range, strings are returned empty.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "lunar_calendar.h"

#define START_YEAR 2026
#define END_YEAR   2056

/*
 * Auto-generated lunar calendar lookup tables (2026-2056).
 * Source: tools/gen_lunar_tables.py
 *
 * Lunar month starts: 383 entries
 * Leap months: [29, 64, 97, 129, 165, 199, 229, 264, 299, 329, 364]
 * Solar terms: 744 entries (744 = 31 years x 24)
 */

/* Day offsets from START_YEAR-01-01 to the 1st day of each lunar month.
 * 383 entries covering lunar years 2026-2056 (12 months × 31 years + 11 leap months). */
static const uint16_t LUNAR_OFFSET_DAYS[] = {
       47,    77,   106,   136,   165,   194,   224,   253,
      282,   312,   342,   372,   401,   431,   461,   490,
      520,   549,   578,   608,   637,   666,   696,   726,
      755,   785,   815,   845,   874,   904,   933,   962,
      992,  1021,  1050,  1080,  1110,  1139,  1169,  1199,
     1228,  1258,  1287,  1317,  1346,  1376,  1405,  1434,
     1464,  1494,  1523,  1553,  1582,  1612,  1642,  1671,
     1701,  1730,  1760,  1789,  1819,  1848,  1877,  1907,
     1937,  1966,  1996,  2025,  2055,  2085,  2114,  2144,
     2173,  2203,  2232,  2262,  2291,  2320,  2350,  2379,
     2409,  2439,  2468,  2498,  2528,  2557,  2587,  2616,
     2646,  2675,  2704,  2734,  2763,  2793,  2822,  2852,
     2882,  2912,  2941,  2971,  3000,  3030,  3059,  3088,
     3118,  3147,  3177,  3206,  3236,  3266,  3295,  3325,
     3355,  3384,  3414,  3443,  3472,  3502,  3531,  3560,
     3590,  3620,  3649,  3679,  3709,  3739,  3768,  3798,
     3827,  3856,  3886,  3915,  3944,  3974,  4003,  4033,
     4063,  4093,  4123,  4152,  4182,  4211,  4240,  4270,
     4299,  4328,  4358,  4387,  4417,  4447,  4477,  4506,
     4536,  4565,  4595,  4624,  4654,  4683,  4712,  4742,
     4771,  4801,  4831,  4860,  4890,  4920,  4949,  4979,
     5008,  5038,  5067,  5097,  5126,  5155,  5185,  5214,
     5244,  5274,  5303,  5333,  5362,  5392,  5422,  5451,
     5481,  5510,  5539,  5569,  5598,  5628,  5657,  5687,
     5717,  5746,  5776,  5806,  5835,  5865,  5894,  5924,
     5953,  5982,  6012,  6041,  6071,  6100,  6130,  6160,
     6189,  6219,  6249,  6278,  6308,  6337,  6366,  6396,
     6425,  6454,  6484,  6514,  6543,  6573,  6603,  6633,
     6662,  6692,  6721,  6750,  6780,  6809,  6838,  6868,
     6897,  6927,  6957,  6987,  7017,  7046,  7076,  7105,
     7134,  7164,  7193,  7222,  7252,  7281,  7311,  7341,
     7371,  7400,  7430,  7459,  7489,  7518,  7548,  7577,
     7606,  7636,  7665,  7695,  7725,  7754,  7784,  7814,
     7843,  7873,  7902,  7932,  7961,  7990,  8020,  8049,
     8079,  8108,  8138,  8168,  8197,  8227,  8257,  8286,
     8316,  8345,  8374,  8404,  8433,  8463,  8492,  8522,
     8551,  8581,  8611,  8640,  8670,  8700,  8729,  8759,
     8788,  8817,  8847,  8876,  8906,  8935,  8965,  8994,
     9024,  9054,  9083,  9113,  9143,  9172,  9202,  9231,
     9260,  9290,  9319,  9348,  9378,  9408,  9437,  9467,
     9497,  9527,  9556,  9586,  9615,  9644,  9674,  9703,
     9732,  9762,  9791,  9821,  9851,  9881,  9911,  9940,
     9970,  9999, 10028, 10058, 10087, 10116, 10146, 10175,
    10205, 10235, 10265, 10294, 10324, 10354, 10383, 10412,
    10442, 10471, 10500, 10530, 10559, 10589, 10619, 10648,
    10678, 10708, 10737, 10767, 10796, 10826, 10855, 10884,
    10914, 10943, 10973, 11002, 11032, 11062, 11092, 11121,
    11151, 11180, 11210, 11239, 11268, 11298, 11327
};
#define LUNAR_OFFSET_COUNT (int)(sizeof(LUNAR_OFFSET_DAYS) / sizeof(LUNAR_OFFSET_DAYS[0]))

/* Indices within LUNAR_OFFSET_DAYS that are leap months.
 * Uses uint16_t because indices can exceed 255 (table has 383 entries). */
static const uint16_t LEAP_MONTH_OFFSETS[] = {29, 64, 97, 129, 165, 199, 229, 264, 299, 329, 364};
#define LEAP_MONTH_COUNT (int)(sizeof(LEAP_MONTH_OFFSETS) / sizeof(LEAP_MONTH_OFFSETS[0]))

/* Day offsets from START_YEAR-01-01 to each solar term.
 * 744 entries: 24 terms × 31 years. */
static const uint16_t SOLAR_TERMS_OFFSETS[] = {
        4,    19,    34,    48,    63,    78,    94,   109,
      124,   140,   155,   171,   187,   203,   218,   234,
      249,   265,   280,   295,   310,   325,   340,   355,
      369,   384,   399,   414,   429,   444,   459,   474,
      490,   505,   521,   536,   552,   568,   584,   599,
      615,   630,   645,   660,   675,   690,   705,   720,
      735,   749,   764,   779,   794,   809,   824,   839,
      855,   870,   886,   902,   917,   933,   949,   964,
      980,   995,  1011,  1026,  1041,  1056,  1070,  1085,
     1100,  1115,  1129,  1144,  1159,  1174,  1189,  1205,
     1220,  1236,  1251,  1267,  1283,  1298,  1314,  1330,
     1345,  1361,  1376,  1391,  1406,  1421,  1436,  1450,
     1465,  1480,  1495,  1509,  1524,  1539,  1555,  1570,
     1585,  1601,  1616,  1632,  1648,  1664,  1679,  1695,
     1710,  1726,  1741,  1756,  1771,  1786,  1801,  1816,
     1830,  1845,  1860,  1875,  1890,  1905,  1920,  1935,
     1951,  1966,  1982,  1997,  2013,  2029,  2045,  2060,
     2076,  2091,  2106,  2121,  2136,  2151,  2166,  2181,
     2196,  2210,  2225,  2240,  2255,  2270,  2285,  2300,
     2316,  2331,  2347,  2363,  2378,  2394,  2410,  2425,
     2441,  2456,  2472,  2487,  2502,  2517,  2531,  2546,
     2561,  2576,  2590,  2605,  2620,  2635,  2650,  2666,
     2681,  2697,  2712,  2728,  2744,  2759,  2775,  2791,
     2806,  2822,  2837,  2852,  2867,  2882,  2897,  2911,
     2926,  2941,  2956,  2970,  2985,  3000,  3016,  3031,
     3046,  3062,  3077,  3093,  3109,  3125,  3140,  3156,
     3171,  3187,  3202,  3217,  3232,  3247,  3262,  3277,
     3291,  3306,  3321,  3336,  3351,  3366,  3381,  3396,
     3411,  3427,  3443,  3458,  3474,  3490,  3505,  3521,
     3537,  3552,  3567,  3582,  3597,  3612,  3627,  3642,
     3657,  3671,  3686,  3701,  3716,  3731,  3746,  3761,
     3777,  3792,  3808,  3824,  3839,  3855,  3871,  3886,
     3902,  3917,  3933,  3948,  3963,  3978,  3992,  4007,
     4022,  4037,  4051,  4066,  4081,  4096,  4111,  4127,
     4142,  4158,  4173,  4189,  4205,  4220,  4236,  4252,
     4267,  4283,  4298,  4313,  4328,  4343,  4358,  4372,
     4387,  4402,  4417,  4431,  4446,  4461,  4477,  4492,
     4507,  4523,  4538,  4554,  4570,  4586,  4601,  4617,
     4632,  4648,  4663,  4678,  4693,  4708,  4723,  4738,
     4752,  4767,  4782,  4797,  4812,  4827,  4842,  4857,
     4872,  4888,  4904,  4919,  4935,  4951,  4966,  4982,
     4998,  5013,  5028,  5043,  5058,  5073,  5088,  5103,
     5118,  5132,  5147,  5162,  5177,  5192,  5207,  5222,
     5238,  5253,  5269,  5285,  5300,  5316,  5332,  5347,
     5363,  5378,  5394,  5409,  5424,  5439,  5453,  5468,
     5483,  5498,  5512,  5527,  5542,  5557,  5572,  5588,
     5603,  5618,  5634,  5650,  5666,  5681,  5697,  5713,
     5728,  5743,  5759,  5774,  5789,  5804,  5819,  5833,
     5848,  5863,  5878,  5892,  5907,  5922,  5937,  5953,
     5968,  5984,  5999,  6015,  6031,  6047,  6062,  6078,
     6093,  6109,  6124,  6139,  6154,  6169,  6184,  6199,
     6213,  6228,  6243,  6258,  6273,  6288,  6303,  6318,
     6333,  6349,  6365,  6380,  6396,  6412,  6427,  6443,
     6459,  6474,  6489,  6504,  6519,  6534,  6549,  6564,
     6579,  6593,  6608,  6623,  6638,  6653,  6668,  6683,
     6699,  6714,  6730,  6746,  6761,  6777,  6793,  6808,
     6824,  6839,  6854,  6870,  6885,  6900,  6914,  6929,
     6944,  6959,  6973,  6988,  7003,  7018,  7033,  7048,
     7064,  7079,  7095,  7111,  7127,  7142,  7158,  7174,
     7189,  7204,  7220,  7235,  7250,  7265,  7280,  7294,
     7309,  7324,  7339,  7353,  7368,  7383,  7398,  7414,
     7429,  7445,  7460,  7476,  7492,  7507,  7523,  7539,
     7554,  7570,  7585,  7600,  7615,  7630,  7645,  7660,
     7674,  7689,  7704,  7719,  7734,  7749,  7764,  7779,
     7794,  7810,  7826,  7841,  7857,  7873,  7888,  7904,
     7920,  7935,  7950,  7965,  7980,  7995,  8010,  8025,
     8040,  8054,  8069,  8084,  8099,  8114,  8129,  8144,
     8160,  8175,  8191,  8206,  8222,  8238,  8254,  8269,
     8285,  8300,  8315,  8331,  8346,  8360,  8375,  8390,
     8405,  8419,  8434,  8449,  8464,  8479,  8494,  8509,
     8525,  8540,  8556,  8572,  8587,  8603,  8619,  8634,
     8650,  8665,  8681,  8696,  8711,  8726,  8741,  8755,
     8770,  8785,  8799,  8814,  8829,  8844,  8859,  8875,
     8890,  8906,  8921,  8937,  8953,  8968,  8984,  9000,
     9015,  9031,  9046,  9061,  9076,  9091,  9106,  9121,
     9135,  9150,  9165,  9180,  9194,  9209,  9225,  9240,
     9255,  9271,  9287,  9302,  9318,  9334,  9349,  9365,
     9380,  9396,  9411,  9426,  9441,  9456,  9471,  9486,
     9500,  9515,  9530,  9545,  9560,  9575,  9590,  9605,
     9621,  9636,  9652,  9667,  9683,  9699,  9715,  9730,
     9746,  9761,  9776,  9792,  9807,  9821,  9836,  9851,
     9866,  9880,  9895,  9910,  9925,  9940,  9955,  9970,
     9986, 10001, 10017, 10033, 10048, 10064, 10080, 10095,
    10111, 10126, 10142, 10157, 10172, 10187, 10202, 10216,
    10231, 10246, 10260, 10275, 10290, 10305, 10320, 10336,
    10351, 10367, 10382, 10398, 10414, 10429, 10445, 10461,
    10476, 10492, 10507, 10522, 10537, 10552, 10567, 10582,
    10596, 10611, 10626, 10641, 10655, 10670, 10686, 10701,
    10716, 10732, 10747, 10763, 10779, 10795, 10810, 10826,
    10841, 10857, 10872, 10887, 10902, 10917, 10932, 10947,
    10961, 10976, 10991, 11006, 11021, 11036, 11051, 11066,
    11082, 11097, 11113, 11128, 11144, 11160, 11176, 11191,
    11207, 11222, 11237, 11253, 11268, 11282, 11297, 11312
};
#define SOLAR_TERMS_COUNT (int)(sizeof(SOLAR_TERMS_OFFSETS) / sizeof(SOLAR_TERMS_OFFSETS[0]))

/* UTF-8 Chinese character tables (each entry is one CJK glyph in UTF-8). */
static const char *TIANGAN[]  = {"甲","乙","丙","丁","戊","己","庚","辛","壬","癸"};
static const char *DIZHI[]    = {"子","丑","寅","卯","辰","巳","午","未","申","酉","戌","亥"};
static const char *LUNAR_MONTHS[] = {"正","二","三","四","五","六","七","八","九","十","冬","腊"};
static const char *LUNAR_DAYS[] = {
    "初一","初二","初三","初四","初五","初六","初七","初八","初九","初十",
    "十一","十二","十三","十四","十五","十六","十七","十八","十九","二十",
    "廿一","廿二","廿三","廿四","廿五","廿六","廿七","廿八","廿九","三十"
};
static const char *SOLAR_TERMS[] = {
    "小寒","大寒","立春","雨水","惊蛰","春分","清明","谷雨",
    "立夏","小满","芒种","夏至","小暑","大暑","立秋","处暑",
    "白露","秋分","寒露","霜降","立冬","小雪","大雪","冬至"
};

static int is_leap_year(uint16_t year)
{
    if (year % 400 == 0) return 1;
    if (year % 100 == 0) return 0;
    if (year % 4 == 0) return 1;
    return 0;
}

/* Day count from START_YEAR-01-01 to (year, month, day). Day 1 of START_YEAR = 0. */
static int32_t get_day_count(uint16_t year, uint8_t month, uint8_t day)
{
    int32_t days = 0;

    for (uint16_t y = START_YEAR; y < year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }

    /* month_days[m] = day-of-year of the 1st of month m (Jan 1 = 0). Index 0 unused. */
    static const uint16_t month_days[] = {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    if (month >= 1 && month <= 12) {
        days += month_days[month];
    }

    if (is_leap_year(year) && month > 2) {
        days += 1;
    }

    days += day - 1;
    return days;
}

void lunar_calendar_convert(uint16_t solar_year, uint8_t solar_month, uint8_t solar_day, lunar_date_t *lunar)
{
    memset(lunar, 0, sizeof(*lunar));
    lunar->jieqi[0] = '\0';

    /* Day of week: 2026-01-01 was Thursday (tm_wday: 0=Sun, 4=Thu). */
    int32_t total_days = get_day_count(solar_year, solar_month, solar_day);
    int dow = (4 + total_days) % 7;
    if (dow < 0) dow += 7;
    lunar->day_of_week = (uint8_t)dow;

    if (solar_year < START_YEAR || solar_year > END_YEAR) {
        /* Outside supported range: leave lunar fields empty. */
        return;
    }

    int32_t target_days = total_days;

    /* --- Find lunar month --- */
    int base_month_idx = -1;
    int last_idx = LUNAR_OFFSET_COUNT - 1;

    if (target_days < LUNAR_OFFSET_DAYS[0]) {
        /* Before the first lunar month of START_YEAR. */
        return;
    }

    if (target_days >= LUNAR_OFFSET_DAYS[last_idx] &&
        target_days <= LUNAR_OFFSET_DAYS[last_idx] + 30) {
        base_month_idx = last_idx;
    } else {
        for (int i = 0; i < LUNAR_OFFSET_COUNT; i++) {
            if (LUNAR_OFFSET_DAYS[i] > target_days) {
                base_month_idx = i - 1;
                break;
            }
        }
    }

    if (base_month_idx < 0) {
        return;
    }

    /* --- Leap-month accounting --- */
    int leap_count = 0;
    int is_leap = 0;
    for (int i = 0; i < LEAP_MONTH_COUNT; i++) {
        if (LEAP_MONTH_OFFSETS[i] < base_month_idx) {
            leap_count += 1;
        } else if (LEAP_MONTH_OFFSETS[i] == base_month_idx) {
            leap_count += 1;
            is_leap = 1;
            break;
        } else {
            break;
        }
    }

    int real_month_offset = base_month_idx - leap_count;
    int lunar_year  = START_YEAR + real_month_offset / 12;
    int lunar_month = real_month_offset % 12 + 1;
    int lunar_day   = target_days - LUNAR_OFFSET_DAYS[base_month_idx] + 1;

    if (lunar_day < 1 || lunar_day > 30) {
        lunar_day = 1;  /* safety */
    }

    lunar->year       = (uint16_t)lunar_year;
    lunar->month      = (uint8_t)lunar_month;
    lunar->day        = (uint8_t)lunar_day;
    lunar->leap_month = (uint8_t)is_leap;

    /* --- Build display strings --- */
    /* Year: 天干地支 + "年". 1900 was 庚子年 → tiangan index = (offset+6)%10, dizhi = offset%12. */
    int tg = (lunar_year - 1900 + 6) % 10;
    if (tg < 0) tg += 10;
    int dz = (lunar_year - 1900) % 12;
    if (dz < 0) dz += 12;
    snprintf(lunar->year_name, sizeof(lunar->year_name), "%s%s年",
             TIANGAN[tg], DIZHI[dz]);

    /* Month name */
    if (lunar_month >= 1 && lunar_month <= 12) {
        if (is_leap) {
            snprintf(lunar->month_name, sizeof(lunar->month_name), "闰%s月",
                     LUNAR_MONTHS[lunar_month - 1]);
        } else {
            snprintf(lunar->month_name, sizeof(lunar->month_name), "%s月",
                     LUNAR_MONTHS[lunar_month - 1]);
        }
    }

    /* Day name */
    if (lunar_day >= 1 && lunar_day <= 30) {
        snprintf(lunar->day_name, sizeof(lunar->day_name), "%s",
                 LUNAR_DAYS[lunar_day - 1]);
    }

    /* --- Solar term (节气) --- */
    for (int i = 0; i < SOLAR_TERMS_COUNT; i++) {
        if (SOLAR_TERMS_OFFSETS[i] == (uint16_t)target_days) {
            /* Today is a solar term. */
            snprintf(lunar->jieqi, sizeof(lunar->jieqi), "%s",
                     SOLAR_TERMS[i % 24]);
            break;
        } else if (SOLAR_TERMS_OFFSETS[i] > (uint16_t)target_days) {
            int days_to_term = SOLAR_TERMS_OFFSETS[i] - target_days;
            if (days_to_term <= 15) {
                snprintf(lunar->jieqi, sizeof(lunar->jieqi), "+%d%s",
                         days_to_term, SOLAR_TERMS[i % 24]);
            } else {
                lunar->jieqi[0] = '\0';
            }
            break;
        }
    }
}

const char *lunar_get_year_name(uint16_t year)
{
    static char buf[20];
    int tg = (year - 1900 + 6) % 10;
    if (tg < 0) tg += 10;
    int dz = (year - 1900) % 12;
    if (dz < 0) dz += 12;
    snprintf(buf, sizeof(buf), "%s%s年", TIANGAN[tg], DIZHI[dz]);
    return buf;
}

const char *lunar_get_month_name(uint8_t month, uint8_t leap)
{
    static char buf[16];
    if (month < 1 || month > 12) return "";
    if (leap) {
        snprintf(buf, sizeof(buf), "闰%s月", LUNAR_MONTHS[month - 1]);
    } else {
        snprintf(buf, sizeof(buf), "%s月", LUNAR_MONTHS[month - 1]);
    }
    return buf;
}

const char *lunar_get_day_name(uint8_t day)
{
    static char buf[8];
    if (day < 1 || day > 30) return "";
    snprintf(buf, sizeof(buf), "%s", LUNAR_DAYS[day - 1]);
    return buf;
}

const char *lunar_get_jieqi(uint16_t year, uint8_t month, uint8_t day)
{
    static char buf[16];
    buf[0] = '\0';

    if (year < START_YEAR || year > END_YEAR) return buf;

    int32_t target_days = get_day_count(year, month, day);

    for (int i = 0; i < SOLAR_TERMS_COUNT; i++) {
        if (SOLAR_TERMS_OFFSETS[i] == (uint16_t)target_days) {
            snprintf(buf, sizeof(buf), "%s", SOLAR_TERMS[i % 24]);
            return buf;
        } else if (SOLAR_TERMS_OFFSETS[i] > (uint16_t)target_days) {
            int days_to_term = SOLAR_TERMS_OFFSETS[i] - target_days;
            if (days_to_term <= 15) {
                snprintf(buf, sizeof(buf), "+%d%s", days_to_term, SOLAR_TERMS[i % 24]);
            }
            return buf;
        }
    }

    return buf;
}
