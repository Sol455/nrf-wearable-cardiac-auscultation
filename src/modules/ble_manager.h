#ifndef _BLE_MANAGER_H_
#define _BLE_MANAGER_H_

#include <stdbool.h>

int ble_init();

int ble_advertise();

void publish_button_state(bool button_state);

#endif