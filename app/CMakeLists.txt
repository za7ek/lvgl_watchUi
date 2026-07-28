cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS {ZEPHYR_BASE})
project(segment34_watch)

target_sources(app PRIVATE
    src/main.c
    src/components/watchface.c
    src/components/segment34.c
    src/components/icons.c
    src/components/lunar_calendar.c
    src/locale/locale.c
    src/theme/theme.c
    src/fonts/lv_font_cjk.c
    src/fonts/lv_font_segments80.c
    src/fonts/lv_font_led.c
)

target_include_directories(app PRIVATE
    src
    src/components
    src/locale
    src/theme
    src/fonts
)
