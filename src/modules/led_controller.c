#include "led_controller.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "../macros.h"

LOG_MODULE_REGISTER(led_controller, LOG_LEVEL_INF);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static struct k_work_delayable blink_work;
static k_timeout_t blink_interval;
static bool blink_state = false;

static void blink_handler(struct k_work_delayable *work)
{
    blink_state = !blink_state;
    gpio_pin_set_dt(&led, blink_state);
    k_work_schedule(&blink_work, blink_interval);
}

int led_controller_init(void)
{
    int ret;
    if (!device_is_ready(led.port)) {
        LOG_ERR("LED port not ready");
        return -1;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    k_work_init_delayable(&blink_work, blink_handler);
    return ret;
}

void led_controller_on(void)
{
    gpio_pin_set_dt(&led, 1);
}

void led_controller_off(void)
{
    gpio_pin_set_dt(&led, 0);
    blink_state = false;
}

void led_controller_toggle(void)
{
    blink_state = !blink_state;
    gpio_pin_set_dt(&led, blink_state);
}

void led_controller_start_blinking(k_timeout_t interval)
{
    blink_interval = interval;
    k_work_schedule(&blink_work, K_NO_WAIT);  
}

void led_controller_stop_blinking(void)
{
    k_work_cancel_delayable(&blink_work);
    led_controller_off();
}
