#include "event_handler.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "modules/led_controller.h"

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
static AudioStream *audio_stream_ptr = NULL;

void event_handler_set_audio_stream(AudioStream *audio_stream) {
    audio_stream_ptr = audio_stream;
}

void event_handler_post(AppEvent evt)
{
    k_msgq_put(&_event_queue, &evt, K_NO_WAIT);
    k_sem_give(&_event_sem);
}

static void handle_event(AppEvent evt)
{
    switch (app_state) {
        case STATE_IDLE:
            led_controller_start_blinking(K_MSEC(500));
            if (evt.type == EVENT_BUTTON_0_PRESS && audio_stream_ptr != NULL) {
                led_controller_stop_blinking();
                led_controller_on();
                int ret = open_wav_for_write(&audio_stream_ptr->wav_config);
                capture_audio(audio_stream_ptr);
                led_controller_off();
                //ble_start_advertising();
                app_state = STATE_STREAMING;
            }
            break;

        // case STATE_ADVERTISING:
        //     if (evt == EVENT_BLE_CONNECTED) {
        //         //led_set_mode(LED_OFF);
        //         app_state = STATE_CONNECTED;
        //     }
        //     break;

        // case STATE_CONNECTED:
        //     if (evt == EVENT_BUTTON_PRESS || evt == EVENT_BLE_START_STREAMING) {
        //         //audio_start_streaming();
        //         //led_set_mode(LED_ON);
        //         app_state = STATE_STREAMING;
        //     }
        //     break;

        case STATE_STREAMING:
            // if (evt.type == EVENT_AUDIO_FINISHED) {
            //     //audio_stop();
            //     //led_set_mode(LED_OFF);
            //     app_state = STATE_CONNECTED;
            // }
            led_controller_start_blinking(K_MSEC(500));
            app_state = STATE_IDLE;
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
