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
    // Signaled by SIGINT handler for in-process wait
    bool            interrupted;
    void *          data;
    pthread_cond_t * wait;
};

PROXYSTUB(int, mcEventRegister, MOTOR motor, int event);
PROXYSTUB(int, mcEventUnregister, MOTOR motor, int event);

extern int
mcDispatchSignaledEvent(struct event_message * event);
extern int
mcDispatchSignaledEventMessage(response_message_t * event);

extern int
mcSignalEvent(Driver * driver, struct event_info *);

extern int
mcSubscribe(motor_t motor, event_t event, int * reg_id, event_cb_t callback);

extern int
mcSubscribeWithData(motor_t motor, event_t event, int * reg_id,
    event_cb_t callback, void * data);

#endif
