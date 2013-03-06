#include "mdrive.h"

#include "events.h"
#include "motion.h"
#include "serial.h"
#include "profile.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

// #include "lib/callback.h"

int
mdrive_move(Driver * self, motion_instruction_t * command) {
    if (self == NULL)
        return EINVAL;
    mdrive_device_t * device = self->internal;

    char buffer[24];
    int steps = mdrive_microrevs_to_steps(device, command->amount),
        urevs = command->amount;

    struct motion_details move_info = {
        .urevs = urevs,
        .pstart = device->position,
        .type = command->type,
        .moving = true
    };

    switch (command->type) {
        case MCABSOLUTE:
            snprintf(buffer, sizeof buffer, "MA %d", steps);
            // XXX: Ensure device position is known
            move_info.urevs = mdrive_steps_to_microrevs(device,
                steps - device->position);
            break;

        case MCRELATIVE:
            snprintf(buffer, sizeof buffer, "MR %d", steps);
            break;

        case MCSLEW:
            if (device->movement.moving && device->movement.type == MCSLEW
                    && device->movement.urevs == urevs)
                // Requested slew rate is already in progress
                return 0;
            snprintf(buffer, sizeof buffer, "SL %d", steps);
            // XXX: Device position will no longer be known
            // TODO: Cancel motion completion callback event if any
            break;

        case MCJITTER:
            // TODO: Lookup jitter label from the microcode

        default:
            return ENOTSUP;
    }

    int status = mdrive_drive_enable(device);
    if (status)
        return status;

    device->movement = move_info;
    clock_gettime(CLOCK_REALTIME, &device->movement.start);

    if (device->microcode.features.move) {
        status = mdrive_move_assisted(device, command, steps);
        if (status)
            return status;
    }
    else {
        mdrive_set_profile(device, &command->profile);
        if (RESPONSE_OK != mdrive_send(device, buffer))
            return EIO;
    }

    return 0;
}

int
mdrive_move_assisted(mdrive_device_t * device, motion_instruction_t * command,
        int steps) {
    struct micrcode_packed_move_op {
        unsigned    mode        :3;
        unsigned    profile     :3;
        unsigned    reset_pos   :1;
        unsigned    steps       :24;
    } R1 = {
        .mode = 0
    };

    switch (command->type) {
        case MCABSOLUTE:
            R1.mode = 1;
            break;
        case MCRELATIVE:
            R1.mode = 2;
            break;
        case MCSLEW:
            R1.mode = 3;
            R1.reset_pos = 1;
            break;
        default:
            return ENOTSUP;
    }

    // Configure hardware profile if current motor profile has the hardware
    // flag set
    if (command->profile.attrs.hardware)
        R1.profile = command->profile.attrs.number;
    else
        mdrive_set_profile(device, &command->profile);

    char buffer[64];
    snprintf(buffer, sizeof buffer, "R1=%d", R1.mode + (R1.profile << 3)
        + (R1.reset_pos << 6));
    if (mdrive_send(device, buffer))
        return EIO;

    snprintf(buffer, sizeof buffer, "R2=%d", steps);
    if (mdrive_send(device, buffer))
        return EIO;

    snprintf(buffer, sizeof buffer, "EX %s", device->microcode.labels.move);
    if (mdrive_send(device, buffer))
        return EIO;

    return 0;
}

int
mdrive_estimate_position_at(mdrive_device_t * device,
        struct motion_details * details, int when) {

    // Travel time is time in microseconds into the device movement. See if
    // we are traveling at vmax, decelerating or still accelerating
    int phase, dt_us;
    long long pos_urev=0;
    // TODO: Add case for motion already completed
    if (details->decel_us < when)
        // Unit is now decelerating
        phase = 3;
    else if (details->vmax_us < when)
        // Unit is traveling at vmax
        phase = 2;
    else 
        // Unit is accelerating
        phase = 1;

    switch (phase) {
        case 3:
            // Add decel ramp time. Change in position is velocity * time.
            // Velocity is vmax - (decel * dt)
            dt_us = when - details->decel_us;
            pos_urev += dt_us
                * (device->profile.decel.value * dt_us)
                / (int)2e6;
        case 2:
            // Add travel time at vmax
            pos_urev += device->profile.vmax.value
                * (min(when, details->decel_us) - details->vmax_us);
        case 1:
            // Add accel ramp time
            dt_us = min(when, details->vmax_us);
            pos_urev += dt_us
                * (device->profile.vstart.value
                    + (device->profile.accel.value * dt_us))
                / (int)2e6;
    }
    return mdrive_microrevs_to_steps(device, pos_urev / (int)1e6);
}

int
mdrive_drive_enable(mdrive_device_t * device) {
    if (!device)
        return EINVAL;
    else if (!device->drive_enabled && !mdrive_set_variable(device, "DE", 1))
        return EIO;
    return 0;
}

int
mdrive_drive_disable(mdrive_device_t * device) {
    if (!device)
        return EINVAL;
    else if (device->drive_enabled && !mdrive_set_variable(device, "DE", 0))
        return EIO;
    return 0;
}

int
mdrive_lazyload_motion_config(mdrive_device_t * device) {
    if (!device->loaded.encoder) {
        // Fetch microstep and encoder settings
        const char * vars[] = { "MS", "EE", "DE" };
        int * vals[] = { &device->steps_per_rev, &device->encoder,
            &device->drive_enabled };
        if (0 != mdrive_get_integers(device, vars, vals, 3))
            return EIO;
 
        // TODO: Check if different part numbers have varying steps per
        //       revolution
        device->steps_per_rev *= 200;
        device->loaded.encoder = true;
        device->loaded.enabled = true;
    }
    return 0;
}

int
mdrive_microrevs_to_steps(mdrive_device_t * device, long long microrevs) {
    int steps_per_rev;

    if (mdrive_lazyload_motion_config(device))
        return -EIO;
    
    if (device->encoder)
        steps_per_rev = 2048;
    else
        steps_per_rev = device->steps_per_rev;

    return (microrevs * steps_per_rev) / (int)1e6;
}

long long
mdrive_steps_to_microrevs(mdrive_device_t * device, int steps) {
    int steps_per_rev;

    if (mdrive_lazyload_motion_config(device))
        return -EIO;
    
    if (device->encoder)
        steps_per_rev = 2048;
    else
        steps_per_rev = device->steps_per_rev;

    return ((long long)steps * (long long)1e6) / steps_per_rev;
}

int
mdrive_stop(Driver * self, enum stop_type type) {
    if (self == NULL || self->internal == NULL)
        return EINVAL;

    mdrive_device_t * device = self->internal;
    mdrive_device_t global;

    // Unit won't be moving any more
    bzero(&device->movement, sizeof device->movement);

    switch (type) {
        case MCSTOP:
            return mdrive_send(device, "SL 0");
        case MCHALT:
            // Handle non-addressed mode (ES) where multiple responses will
            // be received from this command.
            return mdrive_send(device, "\x1b");
        case MCESTOP:
            global = *device;
            global.address = '*';
            // XXX: Detect if the targeted device does not have party mode
            // XXX: Send with checksum toggled too, for safety
            if (mdrive_send(&global, "\x1b"))
                return EIO;
            // XXX: DE the motors too
            if (mdrive_send(&global, "DE=0"))
                return EIO;
        default:
            return ENOTSUP;
    }
}

int
mdrive_home(Driver * self, enum home_type type, enum home_direction dir) {
    if (self == NULL || self->internal == NULL)
        return EINVAL;

    mdrive_device_t * motor = self->internal;

    switch (type) {
        case MCHOMEDEF:
            // XXX: Move to configuration or to firmware:
            // "EX CF" -> "xx xx M1 ..." <-- #3 is homing label
            return mdrive_send(motor, "EX M1");
        case MCHOMESTOP:
            // TODO: Home to hard stop, use microcode if supported
        default:
            return ENOTSUP;
    }
}

