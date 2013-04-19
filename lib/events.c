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

// Server-side event subscription list
static struct subscribe_list * subscriptions = NULL;
static pthread_mutex_t subscription_lock = PTHREAD_MUTEX_INITIALIZER;

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
        .callback = callback,
        .data = data
    };
    client_callback_count++;

    *reg_id = e->id;

    // Async receive events
    if (mcClientCallModeGet() == MC_CALL_CROSS_PROCESS)
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

static int
mcDropEventSubscription(MOTOR motor, int event) {
    // Search for given registration
    pthread_mutex_lock(&subscription_lock);
    struct subscribe_list * current = subscriptions;

    while (current) {
        if (current->event == event && current->motor->id == motor) {
            // Remove the link from the list -- middle of the list
            if (current->prev && current->next) {
                current->prev->next = current->next;
                current->next->prev = current->prev;
            }
            // end of the list
            else if (current->prev) {
                current->prev->next = NULL;
            }
            // beginning of a list with two or more items
            else if (current == subscriptions && current->next) {
                subscriptions = current->next;
                subscriptions->prev = NULL;
            }
            // beginning and only item in the list
            else if (current == subscriptions)
                subscriptions = NULL;

            free(current);
            break;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&subscription_lock);

    // XXX: Unsubscribe from driver if no clients remain subscribed to this
    //      event id

    return (current) ? 0 : EINVAL;
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
    if (info->event > EV__LAST || info->event < EV__FIRST)
        // Strange
        return -1;

    struct event_message evt = {
        .event = info->event,
        .id = 1,
    };

    if (info->data)
        evt.data = *info->data;

    int status=0;

    // Walk the subscription list to find subscriptions that match the
    // event-id and motor-driver event being signaled.
    struct subscribe_list * current = subscriptions, copy;
    pthread_mutex_lock(&subscription_lock);
    while (current) {
        // Ensure the event-type and motor-driver match. In terms of
        // subscription active/inactive and callback information, that's all
        // handled client-side. So the client will always receive the event
        // as long as it is subscribed, it's up to the client to deliver the
        // event or not.
        if (current->event == info->event
                && current->motor->driver == driver
                && current->motor->active) {
            evt.motor = current->motor->id;
            // Use the registration ->inproc flag to determine if the client
            // was in-process when it registered for the event. Call with
            // the list unlocked in case the client changes the registration
            // in its event handler
	    // NOTE: That current might be free()d while the lock is open.
	    //       Therefore, operate on a copy of current
	    copy = *current;
            current = &copy;
            pthread_mutex_unlock(&subscription_lock);
            if (current->inproc)
                status = mcDispatchSignaledEvent(&evt);
            else {
                status = mcEventSend(current->motor->client_pid, &evt);
                if (status < 0) {
                    mcTraceF(10, LIB_CHANNEL,
                        "Unable to send event to client: %s",
                        strerror(-status));
                    // Client went away -- and didn't say bye!
                    mcInactivate(current->motor);
                    mcDropEventSubscription(current->motor->id, current->event);
                }
            }
            pthread_mutex_lock(&subscription_lock);
        }
        current = current->next;
    }

    pthread_mutex_unlock(&subscription_lock);

    // XXX: Reregistration might be necessary, if requested by the
    //      subscriber. Otherwise, the event entry should be marked
    //      inactive.

    return status;
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
int
PROXYIMPL(mcEventRegister, MOTOR motor, int event) {

    if (event > EV__LAST || event < EV__FIRST)
        return EINVAL;

    // Subscribe to events received from the motor
    if (!SUPPORTED(CONTEXT->motor, subscribe))
        return ENOTSUP;

    // XXX: Consider return status from driver
    INVOKE(CONTEXT->motor, subscribe, event, mcSignalEvent);

    struct subscribe_list * info = malloc(sizeof(struct subscribe_list));
    *info = (struct subscribe_list) {
        .event = event,
        .motor = CONTEXT->motor,
        .inproc = CONTEXT->inproc,
    };

    struct subscribe_list * current = subscriptions, * tail = NULL;
    pthread_mutex_lock(&subscription_lock);

    while (current) {
        tail = current;
        current = current->next;
    }
    // End of list
    if (tail) {
        tail->next = info;
        info->prev = tail;
    }
    // Empty list?
    else if (!subscriptions)
        subscriptions = info;

    pthread_mutex_unlock(&subscription_lock);

    return 0;
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
int
PROXYIMPL(mcEventUnregister, MOTOR motor, int event) {

    if (event > EV__LAST || event < EV__FIRST)
        return EINVAL;

    return mcDropEventSubscription(motor, event);
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
    return 0;
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

    int status = 0, size;

    response_message_t msg = {0};
    struct event_message * evt;

    do {
        do {
            msg.payload_size = 0;
            size = mcResponseReceive2(&msg, false, NULL);
            if (size == -EINTR)
                goto finish;
        } while (msg.payload_size == 0);

        evt = (void *) msg.payload;
        if (evt->motor == motor && evt->event == event)
            break;
    } while (true);

    // Mark event registration active (if not unsubscribed, which might make
    // reg be a registration for something else, now)
    if (reg->motor == motor && reg->event == event)
        reg->waiting = false;

finish:
    if (size == -EINTR)
        raise(SIGINT);

    // Awaited event received
    return status;
}

/**
 * mcEventWaitForCondIntr
 *
 * Signal handler used by mcEventWaitForCondition, since pthread_cond_wait
 * will not return from a SIGINT interruption. This handler, when explicity
 * configured to handle an interruption signal such as SIGINT, will set the
 * ->interrupted flag on all waiting conditions
 */
static void
mcEventWaitForCondIntr(int signal) {
    struct client_callback * reg = events;
    while (reg && reg->motor) {
        if (reg->waiting && reg->wait) {
            reg->interrupted = true;
            pthread_cond_signal(reg->wait);
        }
        reg++;
    }
}

static int
mcEventWaitForCondition(motor_t motor, event_t event) {
    struct client_callback * reg = events;

    // Find the event the caller is requesting a wait on
    while (reg && reg->motor) {
        if (motor == reg->motor && event == reg->event
                && reg->active) {
            reg->waiting = true;
            reg->interrupted = false;
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

    // Trap the SIGINT signal to abort the wait
    struct sigaction save, catch = { .sa_handler = mcEventWaitForCondIntr };
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
    sigaction(SIGINT, &catch, &save);

    reg->wait = &signal;

    pthread_mutex_lock(&useless);
    int status;
    while (true) {
        status = pthread_cond_wait(&signal, &useless);
        if (status != 0)
            break;
        // SIGINT handler will set interrupted flag
        else if (reg->interrupted)
            break;
        // Signaler will clear waiting flag
        else if (!reg->waiting)
            break;
    }

    pthread_cond_destroy(&signal);

    // Done. Cleanup and return
    pthread_mutex_unlock(&useless);
    pthread_mutex_destroy(&useless);

    reg->wait = NULL;
    reg->waiting = false;

    // Cleanup SIGINT trapping
    pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
    sigaction(SIGINT, &save, NULL);
    // Raise SIGINT to the default handler
    if (reg->interrupted)
        raise(SIGINT);

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
