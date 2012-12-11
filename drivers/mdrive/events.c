#include "mdrive.h"

#include "config.h"

#include <errno.h>
#include <stdio.h>

// Cross reference between motor error codes reported in event notifications
// and the event constants defined in mcontrol's motor.h
static struct event_xref {
    short           event_code;
    enum event_name event_name;
} event_xrefs[] = {
    { MDRIVE_ESTALL,    EV_MOTOR_STALLED },
    { 200,              EV_MOTOR_RESET },
    { 0, 0 }
};

int
mdrive_subscribe(Driver * self, driver_event_callback_t callback) {
    mdrive_axis_t * axis = self->internal;

    if (axis == NULL)
        return EINVAL;

    if (axis->subscribers >= MAX_SUBSCRIPTIONS - 1)
        return ER_TOO_MANY;

    event_callback_t * event = &axis->event_handlers[axis->subscribers++];

    event->callback = callback;
    event->active = true;

    return 0;
}

int
mdrive_unsubscribe(Driver * self, driver_event_callback_t callback) {
    mdrive_axis_t * axis = self->internal;

    if (axis == NULL)
        return EINVAL;

    if (axis->subscribers == 0)
        return EINVAL;

    int i;
    for (i=0; i < axis->subscribers; i++)
        if (axis->event_handlers[i].callback == callback)
            break;

    if (i == axis->subscribers)
        // Callback function not registered
        return EINVAL;

    // Remove the subscriber from the list by shifting the following items
    // over the callback to be removed
    while (i < axis->subscribers) {
        axis->event_handlers[i] = axis->event_handlers[i+1];
        i++;
    }

    axis->subscribers--;

    return 0;
}

/**
 * mdrive_signal_event
 *
 * Used internally to broadcast an event to all registered event callbacks.
 * The event received should correspond to an event or error code listed in
 * the event_xrefs[] array defined at the top of this module.
 *
 * Returns:
 * (int) EINVAL if AXIS is null or the mdrive event code does not have a
 *       corresponding mcontrol event code, 0 otherwise
 */
int
mdrive_signal_event(mdrive_axis_t * axis, int event) {
    if (axis == NULL)
        return EINVAL;

    struct event_xref * xref = event_xrefs;
    int i;
    while (xref->event_code && xref->event_code != event)
        xref++;

    if (xref->event_code == 0)
        // Event code does not match a defined event name
        return EINVAL;

    // TODO: Send "EV=0" to the unit to signal the receipt of the event

    // Update stats for interesting events
    switch (event) {
        case EV_MOTOR_STALLED:
            axis->stats.stalls++;
            break;
        case EV_MOTOR_RESET:
            axis->stats.reboots++;
            mdrive_config_inspect(axis);
            // Reset lazy loaded flags
            axis->loaded.mask = 0;
            break;
    }

    // Broadcast the event to all (active) subscribers
    for (i=0; i < axis->subscribers; i++)
        if (axis->event_handlers[i].active)
            axis->event_handlers[i].callback(axis->driver, xref->event_name);
    
    return 0;
}

/**
 * mdrive_signal_event_device
 *
 * Used when the axis signaling the event is unknown. This is most likely
 * from the async receive thread that will receive events for multiple
 * devices in party mode.
 *
 * The routine will query mcEnumDrivers from the connected mcontrol library
 * in order to get all active instances of the mdrive device driver. For
 * each, the ->name member will be queried against the address indicated.
 * For the driver that matches, the mdrive_signal_event routine will be
 * called to trigger the event.
 *
 * Returns:
 * (int) EINVAL if no device driver is found servicing the given address,
 *       otherwise, the value from mdrive_signal_event is returned.
 */
int
mdrive_signal_event_device(mdrive_axis_device_t * device, char address,
        int event) {
    Driver * d;
    mdrive_axis_t * axis;
    void * handle;
    handle = mcEnumDrivers();

    while (true) {
        d = mcEnumDriversNext(&handle);
        if (!d || !d->class || !d->class->name)
            break;

        if (strcmp(d->class->name, "mdrive") == 0) {
            axis = d->internal;
            if (axis->address == address)
                return mdrive_signal_event(axis, event);
        }
    }

    return EINVAL;
}

int
mdrive_event_notify(Driver * self, int event, int arg,
        driver_event_callback_t callback) {
    mdrive_axis_t * axis = self->internal;

    if (axis == NULL)
        return EINVAL;

    // TODO: Manage list of subscriptions based on event type. For instance,
    //       for MDrive systems, only one trip on position can be setup at a
    //       time. Therefore, if two clients are waiting on different
    //       positions, that will be a problem.
    switch (event) {
        case EV_POSITION:
            break;
        case EV_INPUT:
            break;
    }
}
