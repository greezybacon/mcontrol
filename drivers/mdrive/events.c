#include "mdrive.h"

#include "events.h"

#include "config.h"
#include "serial.h"

#include <errno.h>
#include <stdio.h>

// Cross reference between motor error codes reported in event notifications
// and the event constants defined in mcontrol's motor.h
static struct event_xref {
    short           error;
    enum event_name event_code;
} event_xrefs[] = {
    { MDRIVE_ESTALL,    EV_MOTION },
    { MDRIVE_ETEMP,     EV_OVERTEMP },
    { 200,              EV_MOTOR_RESET },
    { 0, 0 }
};

int
mdrive_notify(Driver * self, event_t code, int condition,
        driver_event_callback_t callback) {
    mdrive_device_t * device = self->internal;

    if (device == NULL)
        return EINVAL;

    if (callback == NULL)
        return EINVAL;

    struct event_callback * event = device->event_handlers;
    int i = MAX_SUBSCRIPTIONS;
    for (; i; i--, event++)
        if (!event->active) break;

    if (i == 0)
        return ER_TOO_MANY;

    *event = (struct event_callback) {
        .callback = callback,
        .active = true,
        .condition = condition,
        .event = code
    };

    // TODO: Setup special traps for events, especially wrt. the condition
    // variable (eg. event at absolute position)
    switch (code) {
        case EV_POSITION:
            break;
        case EV_INPUT:
            break;
        default:
            break;
    }
    
    return 0;
}

int
mdrive_unsubscribe(Driver * self, driver_event_callback_t callback) {
    mdrive_device_t * device = self->internal;

    if (device == NULL)
        return EINVAL;

    int i = MAX_SUBSCRIPTIONS;
    struct event_callback * event = device->event_handlers;
    for (; i; i--, event++)
        if (event->callback == callback)
            break;

    if (i == 0)
        // Callback function not registered
        return EINVAL;

    // Remove the subscriber from the list by setting the active flag to false
    device->event_handlers[i].active = false;

    return 0;
}

/**
 * mdrive_signal_error_event
 *
 * Convenience method to signal an event based on an error code from a
 * device. This function will call and return the value from
 * mdrive_signal_event if the error code received has a matching event code.
 * This function will also keep track of error-related statistics, such as
 * stalls.
 *
 * Returns:
 * (int) 0 upon successful lookup, EINVAL otherwise
 */
int
mdrive_signal_error_event(mdrive_device_t * device, int error) {
    if (device == NULL)
        return EINVAL;

    struct event_xref * xref = event_xrefs;
    while (xref->error && xref->error != error)
        xref++;

    if (xref->event_code == 0)
        // Error code does have a corresponding event
        return EINVAL;

    union event_data data = {};
    int temp;

    // Update stats for interesting events
    switch (error) {
        case MDRIVE_ESTALL:
            device->stats.stalls++;
            data.motion.stalled = true;
            // Clear the stall flag on the device
            mdrive_send(device, "ST");
            // Device is no longer moving
            device->movement.moving = false;
            break;

        case MDRIVE_ETEMP:
            mdrive_get_integer(device, "IT", &temp);
            mcTraceF(10, MDRIVE_CHANNEL,
                "WARNING: %c: Unit reports over temperature: %d",
                device->address, temp);
            break;
    }
    int status = mdrive_signal_event(device, xref->event_code, &data);
    return status;
}

/**
 * mdrive_signal_event
 *
 * Used internally to broadcast an event to all registered event callbacks.
 * The event received should correspond to an event or error code listed in
 * the event_xrefs[] array defined at the top of this module.
 *
 * Parameters:
 * device - (mdrive_device_t *) Device signaling the event
 * code - (int) event code to be signaled. Use codes from {enum event_name}.
 * data - (union event_data *) information to be coupled with the event
 *      code. Can be set to NULL if no other data is pertinent
 *
 * Returns:
 * (int) EINVAL if AXIS is null or the mdrive event code does not have a
 *       corresponding mcontrol event code, 0 otherwise
 */
int
mdrive_signal_event(mdrive_device_t * device, int code, union event_data * data) {
    if (device == NULL)
        return EINVAL;

    // TODO: Send "EV=0" to the unit to signal the receipt of the event
    // Update stats for interesting events
    switch (code) {
        case MDRIVE_RESET:
            device->stats.reboots++;
            mdrive_config_inspect(device, true);
            // Reset lazy loaded flags
            device->loaded.mask = 0;
            break;
    }

    // Broadcast the event to all (active) subscribers
    struct event_callback * event = device->event_handlers;
    struct event_info info = {
        .event = code,
        .data = data
    };
    int i = MAX_SUBSCRIPTIONS;
    for (; i; i--, event++) {
        if (event->active && !event->paused && event->event == code) {
            event->callback(device->driver, &info);
            // Subscriber will have to request notification for the same
            // type of event again
            event->active = false;
        }
    }
    
    return 0;
}

/**
 * mdrive_signal_event_device
 *
 * Used when the device signaling the event is unknown. This is most likely
 * from the async receive thread that will receive events for multiple
 * devices in party mode.
 *
 * The routine will query mcEnumDrivers from the connected mcontrol library
 * in order to get all active instances of the mdrive device driver. For
 * each, the ->name member will be queried against the address indicated.
 * For the driver that matches, the mdrive_signal_event routine will be
 * called to trigger the event.
 *
 * Parameters:
 * event - (int) event code to signal -- use mdrive_error_to_event() to
 *      convert between Mcode errors and mcontrol event codes
 *
 * Returns:
 * (int) EINVAL if no device driver is found servicing the given address,
 *       otherwise, the value from mdrive_signal_event is returned.
 */
int
mdrive_signal_event_device(mdrive_comm_device_t * comm, char address,
        int event) {
    Driver * d;
    mdrive_device_t * device;
    void * handle;
    handle = mcEnumDrivers();

    while (true) {
        d = mcEnumDriversNext(&handle);
        if (!d || !d->class || !d->class->name)
            break;

        if (strcmp(d->class->name, "mdrive") == 0) {
            device = d->internal;
            if (device->address == address)
                return mdrive_signal_event(device, event, NULL);
        }
    }

    return EINVAL;
}
