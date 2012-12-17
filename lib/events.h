#ifndef EVENTS_H
#define EVENTS_H

#include "../motor.h"
#include "../drivers/driver.h"
#include "message.h"

#include <pthread.h>

typedef struct client_callback client_callback_t;
struct client_callback {
    motor_t         motor;
    event_t         event;
    int             id;
    event_cb_t      callback;
    bool            active;
    bool            waiting;
    void *          data;
    pthread_cond_t * wait;
};

PROXYDEF(mcEventRegister, int, int event);
PROXYDEF(mcEventUnregister, int, int event);

extern int
mcDispatchSignaledEvent(response_message_t * event);

extern int
mcSignalEvent(Driver * driver, struct event_info *);

extern int
mcSubscribe(motor_t motor, event_t event, int * reg_id, event_cb_t callback);

extern int
mcSubscribeWithData(motor_t motor, event_t event, int * reg_id,
    event_cb_t callback, void * data);

#endif
