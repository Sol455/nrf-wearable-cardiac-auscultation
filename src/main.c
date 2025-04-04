#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);


int main(void)
{

    if (!device_is_ready(led0.port)) {
        printk("LED0 GPIO port not ready!\n");
        return 0;
    }

    gpio_pin_configure_dt(&led0, GPIO_OUTPUT);

    while (1) {
        gpio_pin_toggle_dt(&led0);
        k_msleep(200);
    }
}
