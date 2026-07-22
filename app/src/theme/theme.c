#include "theme.h"

static theme_color_t current_theme = THEME_GREEN;

static const theme_colors_t themes[] = {
    {
        .name = "green",
        .bg = LV_COLOR_MAKE(0x0a, 0x16, 0x28),
        .clock_on = LV_COLOR_MAKE(0x00, 0xff, 0x88),
        .clock_off = LV_COLOR_MAKE(0x0a, 0x2a, 0x18),
        .text = LV_COLOR_WHITE,
        .accent = LV_COLOR_MAKE(0xff, 0xaa, 0x00),
        .weather = LV_COLOR_MAKE(0x88, 0xcc, 0xff),
        .heart_rate = LV_COLOR_MAKE(0xff, 0x66, 0x66),
        .steps = LV_COLOR_MAKE(0x88, 0xff, 0xcc),
        .battery = LV_COLOR_MAKE(0x00, 0xff, 0x88),
    },
    {
        .name = "blue",
        .bg = LV_COLOR_MAKE(0x0a, 0x16, 0x28),
        .clock_on = LV_COLOR_MAKE(0x00, 0xaa, 0xff),
        .clock_off = LV_COLOR_MAKE(0x0a, 0x1a, 0x2a),
        .text = LV_COLOR_WHITE,
        .accent = LV_COLOR_MAKE(0xff, 0xaa, 0x00),
        .weather = LV_COLOR_MAKE(0x88, 0xcc, 0xff),
        .heart_rate = LV_COLOR_MAKE(0xff, 0x66, 0x66),
        .steps = LV_COLOR_MAKE(0x88, 0xff, 0xcc),
        .battery = LV_COLOR_MAKE(0x00, 0xaa, 0xff),
    },
    {
        .name = "red",
        .bg = LV_COLOR_MAKE(0x1a, 0x0a, 0x10),
        .clock_on = LV_COLOR_MAKE(0xff, 0x44, 0x44),
        .clock_off = LV_COLOR_MAKE(0x2a, 0x0a, 0x10),
        .text = LV_COLOR_WHITE,
        .accent = LV_COLOR_MAKE(0xff, 0xaa, 0x00),
        .weather = LV_COLOR_MAKE(0x88, 0xcc, 0xff),
        .heart_rate = LV_COLOR_MAKE(0xff, 0x66, 0x66),
        .steps = LV_COLOR_MAKE(0x88, 0xff, 0xcc),
        .battery = LV_COLOR_MAKE(0x00, 0xff, 0x88),
    },
    {
        .name = "orange",
        .bg = LV_COLOR_MAKE(0x1a, 0x10, 0x0a),
        .clock_on = LV_COLOR_MAKE(0xff, 0xaa, 0x00),
        .clock_off = LV_COLOR_MAKE(0x2a, 0x1a, 0x0a),
        .text = LV_COLOR_WHITE,
        .accent = LV_COLOR_MAKE(0xff, 0x44, 0x44),
        .weather = LV_COLOR_MAKE(0x88, 0xcc, 0xff),
        .heart_rate = LV_COLOR_MAKE(0xff, 0x66, 0x66),
        .steps = LV_COLOR_MAKE(0x88, 0xff, 0xcc),
        .battery = LV_COLOR_MAKE(0xff, 0xaa, 0x00),
    },
    {
        .name = "purple",
        .bg = LV_COLOR_MAKE(0x1a, 0x0a, 0x28),
        .clock_on = LV_COLOR_MAKE(0xaa, 0x66, 0xff),
        .clock_off = LV_COLOR_MAKE(0x2a, 0x0a, 0x38),
        .text = LV_COLOR_WHITE,
        .accent = LV_COLOR_MAKE(0xff, 0xaa, 0x00),
        .weather = LV_COLOR_MAKE(0x88, 0xcc, 0xff),
        .heart_rate = LV_COLOR_MAKE(0xff, 0x66, 0x66),
        .steps = LV_COLOR_MAKE(0x88, 0xff, 0xcc),
        .battery = LV_COLOR_MAKE(0xaa, 0x66, 0xff),
    },
    {
        .name = "cyan",
        .bg = LV_COLOR_MAKE(0x0a, 0x20, 0x28),
        .clock_on = LV_COLOR_MAKE(0x00, 0xff, 0xff),
        .clock_off = LV_COLOR_MAKE(0x0a, 0x30, 0x38),
        .text = LV_COLOR_WHITE,
        .accent = LV_COLOR_MAKE(0xff, 0xaa, 0x00),
        .weather = LV_COLOR_MAKE(0x88, 0xcc, 0xff),
        .heart_rate = LV_COLOR_MAKE(0xff, 0x66, 0x66),
        .steps = LV_COLOR_MAKE(0x88, 0xff, 0xcc),
        .battery = LV_COLOR_MAKE(0x00, 0xff, 0xff),
    },
};

void theme_init(theme_color_t theme)
{
    current_theme = theme;
}

const theme_colors_t *theme_get_colors(void)
{
    return &themes[current_theme];
}

theme_color_t theme_get_current(void)
{
    return current_theme;
}

void theme_set_current(theme_color_t theme)
{
    if (theme < THEME_COUNT) {
        current_theme = theme;
    }
}

const char *theme_get_name(theme_color_t theme)
{
    if (theme < THEME_COUNT) {
        return themes[theme].name;
    }
    return "";
}