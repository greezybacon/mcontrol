#include "mdrive.h"

#include "motion.h"
#include "serial.h"
#include "profile.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "lib/callback.h"

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
        .type = command->type
    };

    switch (command->type) {
        case MCABSOLUTE:
            snprintf(buffer, sizeof buffer, "MA %d", steps);
            mdrive_set_profile(device, &command->profile);
            break;

        case MCRELATIVE:
            snprintf(buffer, sizeof buffer, "MR %d", steps);
            mdrive_set_profile(device, &command->profile);
            // XXX: Ensure device position is known
            device->movement.urevs = mdrive_steps_to_microrevs(device,
                device->position + steps);
            break;

        case MCSLEW:
            if (device->movement.type == MCSLEW
                    && device->movement.urevs == urevs)
                // Requested slew rate is already in progress
                return 0;
            snprintf(buffer, sizeof buffer, "SL %d", steps);
            mdrive_set_profile(device, &command->profile);
            // XXX: Device position will no longer be known
            break;

        case MCJITTER:
            // TODO: Lookup jitter label from the microcode

        default:
            return ENOTSUP;
    }
    if (RESPONSE_OK != mdrive_send(device, buffer))
        return EIO;
    
    device->movement = move_info;
    clock_gettime(CLOCK_REALTIME, &device->movement.start);

    // Signal completion event
    if (command->type != MCSLEW)
        // XXX: Ensure device position is known
        mdrive_on_async_complete(device, true);

    return 0;
}

/**
 * __urevs_from_midpoint
 *
 * Estimation of total device rotation if it accelerates its rotation from
 * time t=0 until t=t, and then decelerates back to a stop. This function
 * assumes that the unit's VM (max-velocity, profile.vmax) is never reached.
 *
 * The unit's velocity curve will look like
 *
 * v ^       /\
 *   |     /    \
 *   |   /        \
 *   | /            \
 *   +--------+---------> t
 *   t=0      t=t
 *
 * The area under the curve represents the total distance traveled --
 * urevs (micro-rotations) in this case. The unit will accelerate from and
 * decelerate to the unit's VI (initial-velocity, profile.vstart).
 * Therefore, the acceleration half (left-side) will be (1/2 * b * h), or
 * (1/2) * (t - 0) * (V@t=t), where V@t=t is velocity at time t=t,
 * (VI + A * t^2), where A is the unit's acceleration in urevs/sec^2, and
 * the deceleration half (right-side) will be (1/2) * (V@t=t * decel_time),
 * where decel_time is the velocity at t=t minus VI, quantity over the
 * units's deceleration (D), or ((V@t=t - VI) / D).
 *
 * Lastly, the device's intial velocity must also be integrated, since the
 * above triangle graph will technicall sit upon a "box" with the height of
 * the unit's VI.
 *
 * Returns:
 * (double) Estimated unit travel (in urevs) if the unit accelerates from
 * time t=0 until t=t and decelerates back to a stop.
 */
static double
__urevs_from_midpoint(mdrive_axis_t * device, double t) {
    int vel_at_t = device->profile.accel.raw * t;
    double decel_time = (1.0 * vel_at_t) / device->profile.decel.raw;
    return ((vel_at_t >> 1) + device->profile.vstart.raw) * (t + decel_time);
}

/**
 * __decel_lambda
 *
 * By inspection from the above graph, the slope of the unit's position
 * (velocity) at time t=t is the unit's deceleration (D) rate. However, the
 * deceleration value in the profile is considered a positive value, so we
 * will cast it as a negative value here, since geometricly it is a negative
 * slope.
 */
static double
__decel_lambda(mdrive_axis_t * device, double t) {
    return -(double)device->profile.decel.raw;
}

struct newton_raphson_state {
    mdrive_axis_t * device;             /* Device with motion profile */
    double          xn;                 /* Current value in sequence */
    double          answer;             /* (this) - func() == 0 */
    /* Function whose x value at f(x) = answer is sought */
    double (*func)(mdrive_axis_t *, double); 
    double (*prime)(mdrive_axis_t *, double); /* d/dx (func(x)) */
};

/**
 * _newton_raphson_iter
 *
 * Newton's method implemented as an infinite sequence. The function uses a
 * (struct newton_raphson_state) to setup and continue the sequence. The
 * same state should be passed to the function for the initial and
 * subsequent calls.
 *
 * Parameters:
 * state (struct newton_raphson_state *) Ongoing state of the sequence
 *
 * Returns:
 * state->xn (double) Next value of the sequence
 *
 * Side Effects:
 * state is modified and should be passed back into the function for the
 * next item in the sequence to be generated
 */
static double
_newton_raphson_iter(struct newton_raphson_state * state) {
    state->xn -= (state->answer - state->func(state->device, state->xn))
                / state->prime(state->device, state->xn);
    return state->xn;
}

/**
 * _newton_raphson_xcel
 * 
 * Newton-Raphson method implemeted with Euler's sequence accelerator so
 * that it converges in fewer iterations. The sequence accelerator is
 * implemented from a Python example at
 * http://linuxgazette.net/100/pramode.html, in the section "Representing
 * infinite sequences".
 *
 * Parameters:
 * device (mdrive_axis_t *) Mdrive device (passed to func())
 * x0 (double) Initial value of the sequence
 * answer (double) Sought value from func(). Typically this is zero for
 *      Newton's method in a classic sense, but this can be used where
 *      func(t) = answer -or- answer - func(t) = 0
 *      is being sought.
 * tolerance (double) Acceptable difference between func(t) and answer
 * func (*function(device *, t)) Function with an interesting zero
 * prime (*function(device *, t)) Derivative of func()
 *
 * Returns:
 * Value passed to func -- t (time) in this case, where func(t) = answer.
 */
static double
_newton_raphson_xcel(mdrive_axis_t * device, double answer, double x0,
        double tolerance,
        double (*func)(mdrive_axis_t *, double),
        double (*prime)(mdrive_axis_t *, double)) {
    struct newton_raphson_state state = {
        .xn = x0,
        .answer = answer,
        .func = func,
        .prime = prime,
        .device = device
    };
    int rounds;
    double s0, s1, s2, next;
    s0 = _newton_raphson_iter(&state);
    s1 = _newton_raphson_iter(&state);
    s2 = _newton_raphson_iter(&state);
    do {
        rounds = 4;
        while (rounds--) {
            next = s2 - (((s2 - s1) * (s2 - s1)) / (s0 - (2 * s1) + s2));
            s0 = s1;
            s1 = s2;
            s2 = _newton_raphson_iter(&state);
        }
    } while (abs(answer - func(device, next)) > tolerance);
    return next;
}

int
mdrive_project_completion(mdrive_axis_t * device,
        struct motion_details * details) {
    // Acceleration ramp
    long long dividend =
        (long long)1e9 * (device->profile.vmax.raw - device->profile.vstart.raw);
    long long t1 = dividend / device->profile.accel.raw;
    int avg = (device->profile.vmax.raw + device->profile.vstart.raw) / 2;
    int distance = avg * t1 / (int)1e9;

    // Deceleration ramp
    long long t3 = dividend / device->profile.decel.raw;
    distance += avg * t3 / (int)1e9;

    // And the remaining distance, performed at vmax
    signed long long rem = abs(details->urevs) - distance;

    // Now total time (in nano-seconds) is t1 + t2 + t3
    long long t2, total;

    if (rem < 0) {
        // Unit will never reach vmax and will start decelerating at 50% of
        // the total distance. Estimate the travel time by numerical
        // analysis
        int t = 1e6 * _newton_raphson_xcel(device,
            /* (this) - func() == 0 */ abs(details->urevs),
            /* x0 -- half of A ramp */ t1 / 2e9,
            /* Accept tolerance of 100urevs */ 100,
            __urevs_from_midpoint, __decel_lambda);
        //
        // Then, t is where the curves meet, and t_finish can be found from
        // t_decel = (accel * t + vstart) / decel 
        // t_total = t + t_decel
        //
        // But we just need t -- where the unit will start decelerating
        details->vmax_us = details->decel_us = t;
        // Recalc t1 and t3 times:
        // t1 = Accel time: t * 1000
        // t3 = Decel time: velocity-at-t / decel
        t3 = (device->profile.vstart.raw
            + (device->profile.accel.raw * t))
            / device->profile.decel.raw;
        total = (t + t3) * 1000;
    }
    else {
        t2 = ((long long)1e9 * rem) / device->profile.vmax.raw;

        details->vmax_us = t1 / 1000;
        details->decel_us = (t1 + t2) / 1000;
        total = t1 + t2 + t3;
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
                * (device->profile.decel.raw * dt_us)
                / (int)2e6;
        case 2:
            // Add travel time at vmax
            pos_urev += device->profile.vmax.raw
                * (min(when, details->decel_us) - details->vmax_us);
        case 1:
            // Add accel ramp time
            dt_us = min(when, details->vmax_us);
            pos_urev += dt_us
                * (device->profile.vstart.raw
                    + (device->profile.accel.raw * dt_us))
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
void *
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

    const char * vars[] = {"ST", "P", "V"};
    int stalled, pos, vel;
    int * vals[] = { &stalled, &pos, &vel };

    if (mdrive_get_integers(device, vars, vals, 3))
        return NULL;

    // Add (half-of) comm latency time (ns -> us)
    travel_time += device->stats.latency / 2000;

    int expected = mdrive_estimate_position_at(device, &device->movement,
        travel_time);
    // Add in starting position
    expected += device->movement.pstart;

    int error = pos - expected;

    // Record the last-known position
    if (vel == 0) {
        device->position = pos;
        // Disarm the timer
        device->trip.completion = false;
        union event_data data = {
            .motion.completed = !stalled,
            .motion.stalled = stalled,
            .motion.position = mdrive_steps_to_microrevs(device, pos),
            .motion.error = error
        };
        mdrive_signal_event(device, EV_MOTION, &data);
    } else
        // TODO: Estimate resting position (and time) of the motor
        device->position = expected;
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
    device->cb_complete =mcCallbackAbs(&exp, mdrive_async_completion_correct,
        device);

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
    mdrive_axis_t * axis = self->internal;
    mdrive_axis_t global = *axis;

    switch (type) {
        case MCSTOP:
            return mdrive_send(axis, "SL 0");
        case MCHALT:
            return mdrive_send(axis, "\x1b");
        case MCESTOP:
            global.address = '*';
            // XXX: Send with checksum toggled too, for safety
            return mdrive_send(&global, "\x1b");
        default:
            return ENOTSUP;
    }
}
