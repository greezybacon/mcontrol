#include "mdrive.h"

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
    mdrive_axis_t * device = self->internal;

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
            break;

        case MCJITTER:
            // TODO: Lookup jitter label from the microcode

        default:
            return ENOTSUP;
    }

    if (device->microcode.features.move)
        mdrive_move_assisted(device, command, steps);
    else {
        mdrive_set_profile(device, &command->profile);
        if (RESPONSE_OK != mdrive_send(device, buffer))
            return EIO;
    }

    device->movement = move_info;
    clock_gettime(CLOCK_REALTIME, &device->movement.start);

    // Signal completion event
    if (command->type != MCSLEW)
        // XXX: Ensure device position is known
        mdrive_on_async_complete(device, true);

    return 0;
}


int
mdrive_move_assisted(mdrive_axis_t * device, motion_instruction_t * command,
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

struct travel_time_info {
    double  accel_time;
    double  decel_time;
};

/**
 * travel_to_time
 *
 * Estimates the unit's travel time for the specified travel distance.
 *
 * If the unit does not reach the maximum velocity, the unit's velocity
 * curve will look like
 *
 *   v ^
 *     |       /\
 *     |     /    \
 *     |   /        \
 * VI _| /            \
 *     | |              \
 *     +-+------+--------+-> t
 *       t=0   t@VM     t@rest
 *
 * Where the unit will start from VI, accelerate to some maximum velocity
 * and then decelerate to a complete stop. The area under this curve
 * represents the distance traveled, and the total time (t@rest) is what is
 * sought.
 *
 * Patameters:
 * info - (struct travel_time_info *) which will receive the information
 *      about the travel time including the amount of time spent
 *      accelerating and the amount of time spent decelerating.
 *
 * Returns:
 * EINVAL if the unit will reach the current profile's vmax. 0 upon success.
 */
static int
travel_to_time(mdrive_axis_t * device, long long urevs,
        struct travel_time_info * info) {
    double A = device->profile.accel.value;
    double D = device->profile.decel.value;
    double VI = device->profile.vstart.value;
    double VM = sqrt(((urevs * 2 * A * D) - (D * VI * VI)) / (A + D));

    if (VM > device->profile.vmax.value)
        return EINVAL;

    *info = (struct travel_time_info) {
        .accel_time = ((VM - VI) / A),
        .decel_time = (VM / D)
    };
    return 0;
}

int
mdrive_project_completion(mdrive_axis_t * device,
        struct motion_details * details) {
    // Acceleration ramp
    double ramp = device->profile.vmax.value - device->profile.vstart.value,
    // Acceleration ramp and distance traveled during accel
           t1 = ramp / device->profile.accel.value,
           distance = ((ramp / 2) + device->profile.vstart.value) * t1,
    // Time at VM
           t2,
    // Deceleration ramp and distance traveled during decel
           t3 = 1. * device->profile.vmax.value / device->profile.decel.value;
    distance += (device->profile.vmax.value >> 1) * t3;

    // And the remaining distance, performed at vmax
    double rem = fabs(details->urevs) - distance;

    // Now total time (in nano-seconds) is t1 + t2 + t3
    long long total;

    if (rem < 0) {
        // Unit will never reach vmax and will start decelerating at the
        // intersection of the acceleration and deceleration curves.
        struct travel_time_info info;
        if (travel_to_time(device, abs(details->urevs), &info))
            return EINVAL;

        details->vmax_us = details->decel_us = info.accel_time * 1e6;
        total = (info.accel_time + info.decel_time) * 1e9;
    }
    else {
        t2 = rem / device->profile.vmax.value;

        details->vmax_us = t1 * 1e6;
        details->decel_us = (t1 + t2) * 1e6;
        total = (t1 + t2 + t3) * 1e9;
    }

    struct timespec duration = {
        .tv_sec =   total / (int)1e9,
        .tv_nsec =  total % (int)1e9
    };
    tsAdd(&duration, &details->start, &details->projected);

    mcTraceF(30, MDRIVE_CHANNEL,
        "Projected move time for %.3f revs is %dms",
        details->urevs / 1e6, total / (int)1e6);

    return 0;
}

int
mdrive_estimate_position_at(mdrive_axis_t * device,
        struct motion_details * details, int when) {

    // Travel time is time in microseconds into the device movement. See if
    // we are traveling at vmax, decelerating or still accelerating
    int phase, dt_us;
    long long pos_urev=0;
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

/**
 *
 * Called at device latency seconds before the expected completion of a
 * movement. This routine should collect the current device position and
 * compare it against the expected device position. From there, estimated
 * following error can be projected, and the estimated time of completion
 * can be adjusted based on the position error.
 */
static void *
mdrive_async_completion_correct(void * arg) {
    mdrive_axis_t * device = arg;

    device->cb_complete = 0;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    // Estimate error
    // Estimate current position
    // Compute time in travel (in microseconds)
    int travel_time = (now.tv_sec - device->movement.start.tv_sec) * (int)1e6;
    travel_time += (now.tv_nsec - device->movement.start.tv_nsec) / 1000;

    const char * vars[] = {"ST", "P", "V",
        device->microcode.labels.following_error };
    int stalled, pos, vel, error, count = 4,
        * vals[] = { &stalled, &pos, &vel, &error };

    if (!device->microcode.features.following_error)
        count--;

    while (mdrive_get_integers(device, vars, vals, count));

    if (!device->microcode.features.following_error) {
        // Add (half-of) comm latency time (ns -> us)
        travel_time += device->stats.latency / 2000;

        int expected = mdrive_estimate_position_at(device, &device->movement,
            travel_time);

        // Add in starting position
        expected += device->movement.pstart;
        error = pos - expected;
    }

    bool completed = true;

    if (vel != 0) {
        // (perhaps it's safe to) Assume that the unit is decelerating.
        // Currently, the velocity of the unit is given. The unit will
        // continue to decelerate at a constant rate of D which we have in
        // the unit's profile.
        //
        // Find the intersection of the decel curve with the current
        // velocity:
        //   v
        //   ^
        //   |     ------------\
        //   |  - - - - - - - - X
        //   |                  |\
        //   +------------------+-x----------> t
        //                  Now | L Resting time
        //
        // The change in time is the base of the triangle. The slope of the
        // decel line is the unit's decel value, which will also equal the
        // rise over the run or v / dt. Therefore, dt = v / D, if D is
        // considered a positive value.
        double dt = 1. * mdrive_steps_to_microrevs(device, abs(vel))
            / device->profile.decel.value;
        struct timespec callback = {
            .tv_sec = (int)dt,
            .tv_nsec = (int)1e9 * (dt - (int)dt)
        };
        if (callback.tv_sec == 0 && callback.tv_nsec < device->stats.latency)
            // For all intents and purposes, the unit is stopped, because we
            // won't be able to communicate with it again until well after
            // it stops. Estimate the resting position of the unit and move
            // on.  dt is units of seconds and vel is units of steps per
            // second.  The area under the above triangle will be the
            // distance travelled by the unit, which is 1/2 * b * h
            // or 1/2 * dt * v
            device->position = pos + dt * (vel >> 1);
        else {
            mcTraceF(50, MDRIVE_CHANNEL, "Early. Callback in %dns",
		        callback.tv_nsec);
            // Subtract time for device latency
            callback.tv_nsec -= (device->stats.latency >> 1) + 1e6;
            while (callback.tv_nsec < 0) {
                callback.tv_sec--;
                callback.tv_nsec += (int)1e9;
            }
            // Call this routine again at the estimated time of completion
            // (minus about half of the unit's latency)
            device->cb_complete = mcCallback(&callback,
                mdrive_async_completion_correct, device);
            completed = false;
        }
    }
    // Record the last-known position
    else
        // Device is rested and current position is known
        device->position = pos;

    if (completed) {
        // Disarm the timer
        device->trip.completion = false;
        // Notify interested subscribers of motion event
        union event_data data = {
            .motion = {
                .completed = !stalled,
                .stalled = stalled,
                .pos_known = true,
                .position = mdrive_steps_to_microrevs(device, pos),
                .error = error
            }
        };
        mcTrace(10, MDRIVE_CHANNEL, "Signalling EV_MOTION event");
        mdrive_signal_event(device, EV_MOTION, &data);
    }
    return NULL;        // Just for compiler warnings
}

int
mdrive_on_async_complete(mdrive_axis_t * device, bool cancel) {
    // If not currently waiting on the completion of this device, setup a
    // new timeout and callback
    if (device->trip.completion) {
        if (cancel) {
            if (device->cb_complete)
                mcCallbackCancel(device->cb_complete);
            // TODO: Signal motion-complete cancel event
        }
        else
            return EBUSY;
    }

    // Calculate estimated time of completion
    mdrive_project_completion(device, &device->movement);

    // Callback just before expected completion -- back up the device
    // latency time, and calculate error then
    struct timespec exp = device->movement.projected;
    exp.tv_nsec -= device->stats.latency / 2;
    // We'll also need time to communicate with the unit. Assume 15 chars
    // necessary to get the current position and such
    exp.tv_nsec -= mdrive_xmit_time(device->device, 15);
    // Add back 1ms to account for variations in the device latency
    exp.tv_nsec += (int)1e6;

    while (exp.tv_nsec < 0) {
        exp.tv_sec -= 1;
        exp.tv_nsec += (int)1e9;
    }

    // Call mdrive_async_completion_correct at the projected end-time of
    // this motion command
    device->cb_complete = mcCallbackAbs(&exp,
        mdrive_async_completion_correct, device);

    device->trip.completion = true;

    return 0;
}

int
mdrive_lazyload_motion_config(mdrive_axis_t * device) {
    if (!device->loaded.encoder) {
        // Fetch microstep and encoder settings
        const char * vars[] = { "MS", "EE" };
        int * vals[] = { &device->steps_per_rev, &device->encoder };
        if (0 != mdrive_get_integers(device, vars, vals, 2))
            return EIO;
 
        // TODO: Check if different part numbers have varying steps per
        //       revolution
        device->steps_per_rev *= 200;
        device->loaded.encoder = true;
    }
    return 0;
}

int
mdrive_microrevs_to_steps(mdrive_axis_t * device, long long microrevs) {
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
mdrive_steps_to_microrevs(mdrive_axis_t * device, int steps) {
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

    mdrive_axis_t * axis = self->internal;
    mdrive_axis_t global;

    // Unit won't be moving any more
    bzero(&axis->movement, sizeof axis->movement);

    switch (type) {
        case MCSTOP:
            return mdrive_send(axis, "SL 0");
        case MCHALT:
            return mdrive_send(axis, "\x1b");
        case MCESTOP:
            global = *axis;
            global.address = '*';
            // XXX: Detect if the targeted axis does not have party mode
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

    mdrive_axis_t * motor = self->internal;

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

