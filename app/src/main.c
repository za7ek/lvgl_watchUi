#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

#include <lvgl.h>

#include "watchface.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);
/*
 * LVGL is brought up by the Zephyr LVGL module via lvgl_init(), which runs as a
 * SYS_INIT at INIT_LEVEL_APPLICATION (i.e. before main()). It already takes care
 * of lv_init(), creating the display bound to the Zephyr display driver (with
 * rendering buffers allocated), applying the default theme, and wiring up the
 * LVGL tick through lv_tick_set_cb(k_uptime_get_32).
 *
 * Therefore this application must NOT call lv_init()/lv_display_create()/
 * lv_theme_default_init() or set up its own tick timer: doing so creates a
 * second display without rendering buffers that becomes the default display,
 * which makes lv_timer_handler() crash when it tries to render.
 */
int main(void)
{
    LOG_INF("Segment34 Watchface starting...");

    /* Turn on the display — Zephyr displays start blanked by default.
     * Without this, native_sim shows a transparent/empty window. */
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display)) {
        LOG_ERR("Display device not ready");
        return -1;
    }
    display_blanking_off(display);

    watchface_start();

    while (1) {
        lv_timer_handler();
        k_msleep(5);
    }

    return 0;
}
