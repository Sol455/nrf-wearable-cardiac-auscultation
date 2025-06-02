#include "button_handler.h"
#include <zephyr/logging/log.h>


LOG_MODULE_REGISTER(button_handler);

static const struct gpio_dt_spec sw0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static const struct gpio_dt_spec sw1= GPIO_DT_SPEC_GET(SW1_NODE, gpios);
static struct gpio_callback sw0_cb;
static struct gpio_callback sw1_cb;

int init_buttons(gpio_callback_handler_t cb0, gpio_callback_handler_t cb1)
{
    int ret;

    if (!device_is_ready(sw0.port) || !device_is_ready(sw1.port)) {
        LOG_ERR("One or more button GPIO ports not ready!");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&sw0, GPIO_INPUT);
    if (ret != 0) return ret;

    ret = gpio_pin_interrupt_configure_dt(&sw0, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) return ret;

    gpio_init_callback(&sw0_cb, cb0, BIT(sw0.pin));
    gpio_add_callback(sw0.port, &sw0_cb);

    ret = gpio_pin_configure_dt(&sw1, GPIO_INPUT);
    if (ret != 0) return ret;

    ret = gpio_pin_interrupt_configure_dt(&sw1, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) return ret;

    gpio_init_callback(&sw1_cb, cb1, BIT(sw1.pin));
    gpio_add_callback(sw1.port, &sw1_cb);

    LOG_INF("Buttons configured");

    return 0;
}

