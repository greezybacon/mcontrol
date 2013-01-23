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
static int registration_uid = 0;

/**
 * mcSubscribeWithData
 *
 * Same as mcSubscribe, except allows extra data to be saved and delivered
 * with the event to the callback function. The data will be availabe in the
 * "user_data" member of the event_data struct sent to the callback
 * function.
 */
int
mcSubscribeWithData(motor_t motor, event_t event, int * reg_id,
        event_cb_t callback, void * data) {
    if (events == NULL)
        events = calloc(CLIENT_MAX_CALLBACKS+1, sizeof *events);

    if (reg_id == NULL)
        return EINVAL;

    if (client_callback_count == CLIENT_MAX_CALLBACKS)
        return ER_TOO_MANY;

    // Advance to first inactive callback
    struct client_callback * e = events;
    while (e->active) e++;

    *e = (struct client_callback) {
        .motor = motor,
        .id = ++registration_uid,
        .active = true,
        .event = event,
        .in_process = mcClientCallModeGet() == MC_CALL_IN_PROCESS,
        .callback = callback,
        .data = data
    };
    client_callback_count++;

    *reg_id = e->id;

    // Async receive events
    mcAsyncReceive();

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
mcSubscribe(motor_t motor, event_t event, int * reg_id, event_cb_t callback) {
    return mcSubscribeWithData(motor, event, reg_id, callback, NULL);
}

int
mcUnsubscribe(motor_t motor, int eventid) {
    struct client_callback * e = events;

    // Match by motor id, event id, and callback function pointer
    while (e->motor && (e->motor != motor || e->id != eventid))
        e++;

    if (e->motor == 0)
        // No registration found
        return EINVAL;
    else
        e->active = false;

    client_callback_count--;

    if (motor != -1)
        mcEventUnregister(motor, e->event);

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
mcSignalEvent(Driver * driver, struct event_info * info) {
    // Multiplex (Driver *) driver to all motors that point to the driver
    // instance. For each motor, consult the mask of subscribed events by
    // the client. For each client that has subscribed to the received
    // event, use mcEventSend to signal the client of the event.
    if (!info)
        return EINVAL;

    Motor motors[32];
    int count = mcMotorsForDriver(driver, motors, sizeof motors);

    struct event_message evt = {
        .event = info->event
    };

    if (info->data)
        evt.data = *info->data;

    if (info->event > EV__LAST || info->event < EV__FIRST)
        // Strange
        return -1;

    int status;
    Motor * m = motors;

    for (int i=count; i; i--, m++) {
        if (m->subscriptions[info->event] < 1)
            continue;

        if (mcClientCallModeGet() == MC_CALL_IN_PROCESS) {
            // Find the callback
            struct event_message evt = {
                .event = info->event,
                .motor = m->id,
            };
            evt.data = *info->data,
            status = mcDispatchSignaledEvent(&evt);
        }
        else {
            evt.id = 1;
            evt.motor = m->id;
            status = mcEventSend(m->client_pid, &evt);
            if (status < 0)
                // Client went away -- and didn't say bye!
                mcInactivate(m);
        }

        // XXX: Reregistration might be necessary, if requested by the
        //      subscriber. Otherwise, the event entry should be marked
        //      inactive.
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

    // Subscribe to events received from the motor
    if (m->driver->class->notify == NULL)
        RETURN( ENOTSUP );

    m->driver->class->notify(m->driver, args->event, 0, mcSignalEvent);

    m->subscriptions[args->event]++;
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

    m->subscriptions[args->event]--;
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
mcDispatchSignaledEventMessage(response_message_t * event) {

    if (!event)
        return EINVAL;
    else if (!event->payload)
        return EINVAL;

    return mcDispatchSignaledEvent((void *) event->payload);
}

int
mcDispatchSignaledEvent(struct event_message * evt) {
    struct client_callback * reg = events;

    if (!reg)
        // Events not initialized yet
        return 0;

    // Walk to high-water-mark of the callback registrations using the
    // motor_id, which isn't reset on unsubscription
    while (reg->motor) {
        if ((evt->motor == -1 || evt->motor == reg->motor)
                && evt->event == reg->event
                && reg->active) {
            if (reg->callback) {
                struct event_info notify = {
                    .motor = reg->motor,
                    .event = reg->event,
                    .user = reg->data,
                    .data = &evt->data
                };
                // XXX: Should the (struct client_callback) just be passed ?
                reg->callback(&notify);
            }
            if (reg->wait) {
                reg->waiting = false;
                pthread_cond_signal(reg->wait);
            }
        }
        reg++;
    }
}

static int
mcEventWaitForMessage(motor_t motor, event_t event) {
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

    int status = 0;

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGINT);

    // Block the delivery of the async signal to this process
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    siginfo_t info;
    response_message_t msg;
    struct event_message * evt;

    bool available;

    do {
        // Await delivery of the event signal to this thread
        sigwaitinfo(&mask, &info);

        if (info.si_signo == SIGINT) {
            status = EINTR;
            break;
        }
        else if (info.si_code != SI_MESGQ)
            continue;
        else {
            // Drain all events from the inbox
            do {
                if (mcIsMessageAvailable(&available))
                    break;
                else if (!available)
                    break;
                else if (1 > mcResponseReceive2(&msg, false, NULL))
                    break;
            } while (true);
        } 
        evt = (void *) msg.payload;
        if (evt->motor == motor && evt->event == event)
            break;
    } while (true);

    // Allow delivery of the async signal again
    pthread_sigmask(SIG_UNBLOCK, &mask, NULL);

    // Allow the process to be interrupted elsewhere
    if (status == EINTR)
        kill(getpid(), SIGINT);

    // Mark event registration active (if not unsubscribed, which might make
    // reg be a registration for something else, now)
    if (reg->motor == motor && reg->event == event)
        reg->waiting = false;

    // Awaited event received
    return status;
}

static int
mcEventWaitForCondition(motor_t motor, event_t event) {
    struct client_callback * reg = events;

    // Find the event the caller is requesting a wait on
    while (reg && reg->motor) {
        if (motor == reg->motor && event == reg->event
                && reg->active) {
            reg->waiting = true;
            break;
        }
        reg++;
    }

    if (!reg || !reg->motor)
        return EINVAL;

    // Already waiting on this event?
    if (reg->wait)
        return EBUSY;

    pthread_cond_t signal = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t useless = PTHREAD_MUTEX_INITIALIZER;

    reg->wait = &signal;

    pthread_mutex_lock(&useless);
    int status;
    while (true) {
        status = pthread_cond_wait(&signal, &useless);
        if (status != 0)
            break;
        // Signaler will clear waiting flag
        if (!reg->waiting)
            break;
    }

    pthread_cond_destroy(&signal);

    // Done. Cleanup and return
    pthread_mutex_unlock(&useless);
    pthread_mutex_destroy(&useless);

    reg->wait = NULL;
    reg->waiting = false;

    return status;
}

/**
 * mcEventWait
 *
 * Wait for the arrival of a registered event.
 *
 * Returns:
 * (int) EINVAL if no registration was found for the indicated event.
 *       EINTR if the wait was interrupted.  Otherwise, 0 after the event is
 *       received
 */
int
mcEventWait(motor_t motor, event_t event) {
    if (mcClientCallModeGet() == MC_CALL_IN_PROCESS)
        return mcEventWaitForCondition(motor, event);
    else
        return mcEventWaitForMessage(motor, event);
}
