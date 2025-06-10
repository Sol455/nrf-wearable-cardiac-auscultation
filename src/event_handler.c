#include "event_handler.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "audio/audio_stream.h"

LOG_MODULE_REGISTER(event_handler);

K_MSGQ_DEFINE(_event_queue, sizeof(AppEvent), 8, 4);
K_SEM_DEFINE(_event_sem, 0, 1);

typedef enum {
    STATE_IDLE,
    STATE_ADVERTISING,
    STATE_CONNECTED,
    STATE_STREAMING
} AppState;

static AppState app_state = STATE_IDLE;

void event_handler_post(AppEvent evt)
{
    k_msgq_put(&_event_queue, &evt, K_NO_WAIT);
    k_sem_give(&_event_sem);
}

static void handle_event(AppEvent evt)
{
    switch (app_state) {
    case STATE_IDLE:
        if (evt == EVENT_BUTTON_PRESS) {
            ble_start_advertising();
            led_set_mode(LED_BLINK);
            app_state = STATE_ADVERTISING;
        }
        break;

    case STATE_ADVERTISING:
        if (evt == EVENT_BLE_CONNECTED) {
            led_set_mode(LED_OFF);
            app_state = STATE_CONNECTED;
        }
        break;

    case STATE_CONNECTED:
        if (evt == EVENT_BUTTON_PRESS || evt == EVENT_BLE_START_STREAMING) {
            audio_start_streaming();
            led_set_mode(LED_ON);
            app_state = STATE_STREAMING;
        }
        break;

    case STATE_STREAMING:
        if (evt == EVENT_AUDIO_FINISHED || evt == EVENT_BLE_STOP_STREAMING) {
            audio_stop();
            led_set_mode(LED_OFF);
            app_state = STATE_CONNECTED;
        }
        break;
    }
}

int event_handler_run(void)
{
    LOG_INF("Starting event handler loop");

    while (1) {
        k_sem_take(&_event_sem, K_FOREVER);
        AppEvent evt;
        while (k_msgq_get(&_event_queue, &evt, K_NO_WAIT) == 0) {
            handle_event(evt);
        }
    }

    return 0;
}
