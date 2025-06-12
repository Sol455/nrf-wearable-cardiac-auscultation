#ifndef EVENT_HANDLER_H_
#define EVENT_HANDLER_H_

#include "audio/audio_stream.h"

int event_handler_run(void);  

typedef enum {
    EVENT_START_UP,
    EVENT_BUTTON_0_PRESS,
    EVENT_BUTTON_1_PRESS,
    EVENT_BLE_CONNECTED,
    EVENT_BLE_DISCONNECTED,
    EVENT_AUDIO_FINISHED,
    EVENT_BLE_START_STREAMING,
    EVENT_BLE_STOP_STREAMING,
    EVENT_BLE_TOGGLE_LED
} AppEventType;

typedef struct {
    AppEventType type;
    void *data;  
} AppEvent;

void event_handler_post(AppEvent evt);

void event_handler_set_audio_stream(AudioStream *stream);

#endif
