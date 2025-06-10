#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

int event_handler_run(void);  

typedef enum {
    EVENT_BUTTON_0_PRESS,
    EVENT_BUTTON_1_PRESS,
    EVENT_BLE_CONNECTED,
    EVENT_BLE_DISCONNECTED,
    EVENT_AUDIO_FINISHED,
    EVENT_BLE_START_STREAMING,
    EVENT_BLE_STOP_STREAMING,
} AppEventType;

typedef struct {
    AppEventType type;
    void *data;  
} AppEvent;

void event_handler_post(AppEvent evt);

#endif
