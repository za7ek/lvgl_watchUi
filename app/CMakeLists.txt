cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(segment34_watch)

FILE(GLOB app_sources src/*.c)
FILE(GLOB components_sources src/components/*.c)
FILE(GLOB locale_sources src/locale/*.c)
FILE(GLOB theme_sources src/theme/*.c)
FILE(GLOB fonts_sources src/fonts/*.c)

target_sources(app PRIVATE
    ${app_sources}
    ${components_sources}
    ${locale_sources}
    ${theme_sources}
    ${fonts_sources}
)

target_include_directories(app PRIVATE
    src
    src/components
    src/locale
    src/theme
    src/fonts
)