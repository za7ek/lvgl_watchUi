#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>

#include <lvgl.h>

#include "watchface.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define LVGL_TICK_PERIOD_MS 1

static void lv_tick_handler(void)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static int init_lvgl(void)
{
    lv_init();

    const struct device *disp_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(disp_dev)) {
        LOG_ERR("Display device not ready");
        return -ENODEV;
    }

    lv_display_t *disp = lv_zephyr_display_create(disp_dev);
    if (!disp) {
        LOG_ERR("Failed to create display");
        return -ENOMEM;
    }

    lv_theme_t *theme = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_RED),
                                              LV_THEME_DEFAULT_DARK,
                                              &lv_font_montserrat_14);
    lv_disp_set_theme(disp, theme);

    return 0;
}

void main(void)
{
    LOG_INF("Segment34 Watchface starting...");

    int ret = init_lvgl();
    if (ret != 0) {
        LOG_ERR("LVGL init failed: %d", ret);
        return;
    }

    k_timer_init(NULL, lv_tick_handler, NULL);
    k_timer_start(NULL, K_MSEC(LVGL_TICK_PERIOD_MS), K_MSEC(LVGL_TICK_PERIOD_MS));

    watchface_start();

    while (1) {
        lv_timer_handler();
        k_msleep(5);
    }
}