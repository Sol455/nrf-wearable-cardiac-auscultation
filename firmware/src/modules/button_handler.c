#include "button_handler.h"
#include "../event_handler.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(button_handler);

#define SW0_NODE DT_ALIAS(sw0)
//#define SW1_NODE DT_ALIAS(sw1)

static const struct gpio_dt_spec btn0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
//static const struct gpio_dt_spec btn1 = GPIO_DT_SPEC_GET(SW1_NODE, gpios);

static struct gpio_callback btn0_cb_data;
//static struct gpio_callback btn1_cb_data;

static struct k_work_delayable debounce_work0;
//static struct k_work_delayable debounce_work1;

static void debounce_handler_btn0(struct k_work *work)
{
    if (gpio_pin_get_dt(&btn0)) {
        AppEvent ev = { .type = EVENT_BUTTON_0_PRESS };
        event_handler_post(ev);
    }
}

// static void debounce_handler_btn1(struct k_work *work)
// {
//     if (gpio_pin_get_dt(&btn1)) {
//         AppEvent ev = { .type = EVENT_BUTTON_1_PRESS };
//         event_handler_post(ev);    
//     }
// }

static void btn0_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_work_reschedule(&debounce_work0, K_MSEC(50));
}

// static void btn1_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
// {
//     k_work_reschedule(&debounce_work1, K_MSEC(50));
// }

int button_handler_init(void)
{

    if (!device_is_ready(btn0.port) /*|| !device_is_ready(btn1.port)*/) {
        LOG_ERR("Button GPIOs not ready");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&btn0, GPIO_INPUT | GPIO_PULL_DOWN);
    //gpio_pin_configure_dt(&btn1, GPIO_INPUT);

    gpio_pin_interrupt_configure_dt(&btn0, GPIO_INT_EDGE_TO_ACTIVE);
    //gpio_pin_interrupt_configure_dt(&btn1, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&btn0_cb_data, btn0_callback, BIT(btn0.pin));
    //gpio_init_callback(&btn1_cb_data, btn1_callback, BIT(btn1.pin));

    gpio_add_callback(btn0.port, &btn0_cb_data);
    //gpio_add_callback(btn1.port, &btn1_cb_data);

    k_work_init_delayable(&debounce_work0, debounce_handler_btn0);
    //k_work_init_delayable(&debounce_work1, debounce_handler_btn1);

    LOG_INF("Two-button handler initialised");
    return 0;
}


