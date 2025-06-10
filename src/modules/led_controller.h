#ifndef _LED_CONTROLLER_H_
#define _LED_CONTROLLER_H_

#include <zephyr/kernel.h>

int led_controller_init(void);
void led_controller_on(void);
void led_controller_off(void);
void led_controller_toggle(void);
void led_controller_start_blinking(k_timeout_t interval);
void led_controller_stop_blinking(void);

#endif

