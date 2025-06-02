#ifndef _BUTTON_HANDLER_H_
#define _BUTTON_HANDLER_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "../macros.h"

int init_buttons(gpio_callback_handler_t cb0, gpio_callback_handler_t cb1);

#endif