#include "../motor.h"

#include "message.h"
#include "motor.h"
#include "events.h"

#include "client.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

static const int CLIENT_MAX_CALLBACKS = 32;
static struct client_callback * events = NULL;
static int client_callback_count = 0;

/**
 * mcSubscribeWithData
 *
 * Same as mcSubscribe, except allows extra data to be saved and delivered
 * with the event to the callback function. The data will be availabe in the
 * "user_data" member of the event_data struct sent to the callback
 * function.
 */
int
mcSubscribeWithData(motor_t motor, event_t event, event_cb_t callback,
        void * data) {
    if (events == NULL)
        events = calloc(CLIENT_MAX_CALLBACKS+1, sizeof *events);

    if (client_callback_count == CLIENT_MAX_CALLBACKS)
        return ER_TOO_MANY;

    // Advance to first inactive callback
    struct client_callback * e = events;
    while (e->active) e++;
    
    *e = (struct client_callback) {
        .motor = motor,
        .active = true,
        .event = event,
        .callback = callback,
        .data = data
    };
    client_callback_count++;

    // Async receive events
    mcAsyncReceive();

    printf("Motor: %d, waiting for %d\n", motor, event);

    if (motor != -1)
        // Server side registration
        return mcEventRegister(motor, event);
    else
        // Wildcard motor. (EV_TRACE events, for instance are not received
        // per motor_id)
        return 0;
}

/**
 * mcSubscribe
 *
 * Registers with the motor to receive callback of the specified event. The
 * event data will be delivered to the given callback function. If not
 * already started, a thread will be spun up on the client side to wait for
 * events from the motor. A different queue will be established to receive
 * the events from.
 *
 * Returns:
 * (int) - subscription_id which will be set in the (struct event_message)
 *      payload delivered to the event handler routine
 */
int
mcSubscribe(motor_t motor, event_t event, event_cb_t callback) {
    return mcSubscribeWithData(motor, event, callback, NULL);
}

int
mcUnsubscribe(motor_t motor, event_t event, event_cb_t callback) {
    struct client_callback * e = events;

    // Match by motor id, event id, and callback function pointer
    while (e->motor && (e->motor != motor || e->event != event
            || e->callback != callback))
        e++;

    if (e->motor == 0)
        // No registration found
        return EINVAL;
    else
        e->active = false;

    client_callback_count--;

    if (motor != -1)
        mcEventUnregister(motor, event);

    return 0;
}

/**
 * mcSignalEvent
 *
 * Server (daemon) side implementation of the event notification.
 * Ultimately, each driver that is connected should be connected to this
 * routine via the driver's ::subscribe method. When events are signaled by
 * the driver, the event will be broadcast out to connected clients. The
 * mcDispatchSignaledEvent will then be used client-side to dispatch the
 * event to a respective callback function, signal handler, etc.
 */
int
mcSignalEvent(Driver * driver, int event) {
    // Multiplex (Driver *) driver to all motors that point to the driver
    // instance. For each motor, consult the mask of subscribed events by
    // the client. For each client that has subscribed to the received
    // event, use mcEventSend to signal the client of the event.
    Motor motors[32];
    int count = mcMotorsForDriver(driver, motors, sizeof motors);

    struct event_message evt = {
        .event = event,
        // TODO: Add in event data (number|string)
    };

    if (event > EV__LAST || event < EV__FIRST)
        // Strange
        return -1;

    int64_t mask = 1 << event;
    int status;

    for (int i=0; i<count; i++) {
        if (!(motors[i].subscriptions & mask))
            continue;

        evt.id = 1;
        evt.motor = motors[i].id;
        status = mcEventSend(motors[i].client_pid, &evt);
        if (status < 0)
            // Client went away -- and didn't say bye!
            mcDisconnect(motors[i].id);
    }
}

/**
 * mcEventRegister
 *
 * Server side implementation of the event registration process. The Motor
 * structure reserves a bitmask of subscribed events for registered clients.
 * This routine will simply set the appropriate bit in the mask for the
 * requested event.
 *
 * This routine is not intended to be called directly. Use mcSubscribe and
 * friends to subscribe to events.
 *
 * Returns:
 * (int) EINVAL if invalid event or motor was specified
 */
PROXYIMPL(mcEventRegister, int event) {
    UNPACK_ARGS(mcEventRegister, args);

    Motor * m = find_motor_by_id(motor, message->pid);

    if (m == NULL)
        RETURN( EINVAL );

    if (args->event > EV__LAST || args->event < EV__FIRST)
        RETURN( EINVAL );

    m->subscriptions |= 1 << args->event;
    RETURN( 0 );
}

/**
 * mcEventUnregister
 *
 * Server side disconnect from event notification.
 *
 * This routine is not intended to be called directly. Use mcUnsubscribe to
 * cancel client-side event registration
 *
 * Returns:
 * (int) - EINVAL if invalid motor or event is specified. 0 otherwise
 */
PROXYIMPL(mcEventUnregister, int event) {
    UNPACK_ARGS(mcEventUnregister, args);

    Motor * m = find_motor_by_id(motor, message->pid);

    if (m == NULL)
        RETURN( EINVAL );

    if (args->event > EV__LAST || args->event < EV__FIRST)
        RETURN( EINVAL );

    m->subscriptions &= ~ (1 << args->event);
    RETURN( 0 );
}

/**
 * mcDispatchSignaledEvent
 *
 * Client-side implementation of event notification. This routine is called
 * from mcResponseReceive whenever an event is received through the inbox
 * channel.
 */
int
mcDispatchSignaledEvent(response_message_t * event) {
    struct client_callback * reg = events;
    struct event_message * evt = (void *) event->payload;

    // Walk to high-water-mark of the callback registrations using the
    // motor_id, which isn't reset on unsubscription
    while (reg->motor) {
        if ((evt->motor == -1 || evt->motor == reg->motor)
                && evt->event == reg->event
                && reg->active
                && !reg->waiting) {
            if (reg->callback) {
                struct event_data notify = {
                    .motor = reg->motor,
                    .event = reg->event,
                    .user_data = reg->data,
                    .event_data = &evt->data
                };
                // XXX: Should the (struct client_callback) just be passed ?
                reg->callback(&notify);
            }
            else if (reg->wait) {
                pthread_cond_signal(reg->wait);
            }
        }
        reg++;
    }
}

/**
 * mcEventWait
 *
 * Wait for the arrival of a registered event.
 *
 * Returns:
 * (int) EINVAL if no registration was found for the indicated event.
 *       Otherwise, 0 after the event is received
 */
int
mcEventWait(motor_t motor, event_t event) {
    // Find the registration of this event and mark it inactive, which
    // will block the dispatch to the callback function.
    struct client_callback * reg = events;
    while (reg->motor) {
        if (reg->motor == motor && reg->event == event) {
            reg->waiting = true;
            break;
        }
        reg++;
    }
    if (!reg->motor)
        // No registration found
        return EINVAL;

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);

    // Block the delivery of the async signal to this process
    sigprocmask(SIG_BLOCK, &mask, NULL);

    siginfo_t info;
    response_message_t msg;
    struct event_message * evt;
    do {
        // Await delivery of the event signal to this thread
        sigwaitinfo(&mask, &info);

        if (info.si_code != SI_MESGQ)
            continue;
        if (1 > mcResponseReceive2(&msg, false, NULL))
            continue;
        
        evt = (void *) msg.payload;
        if (evt->motor == motor && evt->event == event)
            break;
    } while (true);

    // Allow delivery of the async signal again
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    // Mark event registration active (if not unsubscribed, which might make
    // reg be a registration for something else, now)
    if (reg->motor == motor && reg->event == event)
        reg->waiting = false;

    // Awaited event received
    return 0;
}
