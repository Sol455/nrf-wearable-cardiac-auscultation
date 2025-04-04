#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "modules/sd_card.h"

#define LED0_NODE DT_ALIAS(led0)
#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec sw0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static const struct gpio_dt_spec sw1= GPIO_DT_SPEC_GET(SW1_NODE, gpios);

static struct gpio_callback sw0_cb;
static struct gpio_callback sw1_cb;

LOG_MODULE_REGISTER(main);

void sw0_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    gpio_pin_toggle_dt(&led0);
}

void sw1_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    gpio_pin_toggle_dt(&led0);
}

int main(void)
{
    int ret;

    ret = device_is_ready(led0.port);
    ret = device_is_ready(sw0.port);
    ret = device_is_ready(sw1.port);

    if (!ret) {
        LOG_WRN("LED or Button GPIO port not ready!\n");
        return 0;
    }

    gpio_pin_configure_dt(&led0, GPIO_OUTPUT);

    gpio_pin_configure_dt(&sw0, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&sw0, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&sw0_cb, sw0_callback, BIT(sw0.pin));
    gpio_add_callback(sw0.port, &sw0_cb);

    gpio_pin_configure_dt(&sw1, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&sw1, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&sw1_cb, sw1_callback, BIT(sw1.pin));
    gpio_add_callback(sw1.port, &sw1_cb);

    LOG_INF("Configured Buttons & Interrupts");

    while (1) {
        k_msleep(200);
        LOG_INF("Hello");
    }
}

