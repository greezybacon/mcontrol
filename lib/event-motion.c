#include "motor.h"

#include "callback.h"
#include "events.h"
#include "query.h"
#include "trace.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

struct travel_time_info {
    double  accel_time;
    double  decel_time;
};

/**
 * mcTravelToTime
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
mcTravelToTime(Motor * motor, long long urevs,
        struct travel_time_info * info) {

    // Ensure motor is actually going to move
    if (urevs < motor->profile.accuracy.value) {
        *info = (struct travel_time_info) { .accel_time = 0 };
        return 0;
    }

    double A = motor->profile.accel.value;
    double D = motor->profile.decel.value;
    double VI = motor->profile.vstart.value;
    double VM = sqrt(((urevs * 2 * A * D) - (D * VI * VI)) / (A + D));

    if (VM > motor->profile.vmax.value)
        return EINVAL;

    *info = (struct travel_time_info) {
        .accel_time = ((VM - VI) / A),
        .decel_time = (VM / D)
    };
    return 0;
}

/**
 * mcMoveProjectCompletionTime
 *
 * Estimates the resting time, from the start of the move operation, of the
 * motor for the move descrived in the the given motion_details. Projected
 * time of completion is stored as an absolute (struct timespec) value into
 * the detail->projected member. Also, the parts of the move are broken down
 * in to time spent accelerating and time spent decelerating. Those pieces
 * are saved into the details->vmax_us and details->decel_us respectively.
 * The times will reflect the time when the motor is expected to reach the
 * maximum velocity of the move and the time when the motor is expected to
 * start decelerating. Both figures are in micro-seconds from the start of
 * the move (details->start).
 *
 * If the unit will never reach the profile VMAX, the vmax_us and decel_us
 * times will have the same value, because the unit will start decelerating
 * at the time it reaches the max velocity of the move.
 *
 * Parameters:
 * motor - (Motor *) Motor performing the move
 * details - (struct motion_details *) Contains information about the move,
 *      and will receive the estimated timing characteristics of the move
 *
 * Returns:
 * 0 upon success. EINVAL if unable to determine the resting time.
 */
int
mcMoveProjectCompletionTime(Motor * motor,
        struct motor_motion_details * details) {
    // Acceleration ramp
    double ramp = motor->profile.vmax.value - motor->profile.vstart.value,
    // Acceleration ramp and distance traveled during accel
           t1 = ramp / motor->profile.accel.value,
           distance = ((ramp / 2) + motor->profile.vstart.value) * t1,
    // Time at VM
           t2,
    // Deceleration ramp and distance traveled during decel
           t3 = 1. * motor->profile.vmax.value / motor->profile.decel.value;
    distance += (motor->profile.vmax.value >> 1) * t3;

    // Now total time (in nano-seconds) is t1 + t2 + t3
    long long total, urevs = details->urevs;
    
    // Adjust for absolute position moves
    if (details->type == MCABSOLUTE)
        urevs -= details->pstart;

    // And the remaining distance, performed at vmax
    double rem = fabs(urevs) - distance;

    if (rem < 0) {
        // Unit will never reach vmax and will start decelerating at the
        // intersection of the acceleration and deceleration curves.
        struct travel_time_info info;
        if (mcTravelToTime(motor, abs(urevs), &info))
            return EINVAL;

        details->vmax_us = details->decel_us = info.accel_time * 1e6;
        total = (info.accel_time + info.decel_time) * 1e9;
        if (total < 0)
            total = 0;
    }
    else {
        t2 = rem / motor->profile.vmax.value;

        details->vmax_us = t1 * 1e6;
        details->decel_us = (t1 + t2) * 1e6;
        total = (t1 + t2 + t3) * 1e9;
    }

    struct timespec duration = {
        .tv_sec =   total / (int)1e9,
        .tv_nsec =  total % (int)1e9
    };
    tsAdd(&duration, &details->start, &details->projected);

    mcTraceF(30, LIB_CHANNEL,
        "Projected move time for %.3f revs is %dms",
        urevs / 1e6, total / (int)1e6);

    return 0;
}

static void *
mcMoveTrajectCompletionCheckback(void *);

/**
 * mcMoveTrajectCompletion
 *
 * Estimates the resting time of the motor based on its current position and
 * the move requested of the motor. This routine will setup checkbacks to
 * detect the actual resting time of the motor and will also signal events
 * to indicate when the motion stops and for what reason (fail, stall,
 * arrival at destination, etc.)
 *
 * Context:
 * motor->movement should contain information about the new movement (.type,
 * .pstart and .amount are required)
 */
int
mcMoveTrajectCompletion(Motor * motor) {

    // Compute time of completion into
    mcMoveProjectCompletionTime(motor, &motor->movement);

    // Ultimately, this method is called before the move is requested at the
    // motor. Therefore, the move will actually occur t+latency time from
    // now. Therefore, we should not consider the latency of the motor in
    // the checkback time, because the move will finish approximately at the
    // projected-time+latency. Since the latency occurs in both factors, it
    // doesn't need to be considered.

    // TODO: Check if motor->movement.checkback_id is currently set

    // Request a checkback when the move is estimated to be completed
    if (motor->movement.vmax_us > 1)
        motor->movement.checkback_id = mcCallbackAbs(
            &motor->movement.projected,
            mcMoveTrajectCompletionCheckback, motor);
    else {
        // Signal motion-complete event
        // XXX: Can this always be assumed? Should we always query the motor
        //      to ensure that it isn't really moving still?
        union event_data data = {
            .motion = {
                .completed = true,
                .pos_known = true,
                .position = motor->position,
            }
        };
        struct event_info info = {
            .event = EV_MOTION,
            .data = &data,
        };
        mcTraceF(50, LIB_CHANNEL, "Signalling EV_MOTION event (noop)");
        mcSignalEvent(motor->driver, &info);
        // Since we're assuming the device is not going to move, we should
        // also assume the position of the device is still known
        motor->pos_known = true;
    }

    return 0;
}

/**
 * mcMoveTrajectCompletionCancel
 *
 * Called at the beginning of a move. If a checkback is registered to detect
 * the completion of the move-in-progress, the checkback will be canceled,
 * and an EV_MOTION will be signaled with attached motion data indicating
 * the move was canceled.
 */
int
mcMoveTrajectCompletionCancel(Motor * motor) {
    if (motor->movement.checkback_id == 0)
        return 0;

    mcCallbackCancel(motor->movement.checkback_id);
    motor->movement.checkback_id = 0;

    // Signal motion-complete cancel event
    union event_data data = {
        .motion = {
            .cancelled = true
        }
    };
    struct event_info info = {
        .event = EV_MOTION,
        .data = &data,
    };
    mcTraceF(50, LIB_CHANNEL, "Signalling EV_MOTION event (cancelled)");
    mcSignalEvent(motor->driver, &info);
    return 0;
}

/**
 * (Callback) mcMoveTrajectCompletionCheckback
 *
 * Called at (half of the) device latency seconds before the expected
 * completion of a movement. This routine should collect the current device
 * position and compare it against the expected device position. From there,
 * estimated following error can be projected, and the estimated time of
 * completion can be adjusted based on the position error.
 *
 * If the motor is at rest when the callback is performed (optimal), the
 * EV_MOTION event will be signaled and the details about the move will be
 * included with the event data.
 */
static void *
mcMoveTrajectCompletionCheckback(void * arg) {
    Motor * motor = arg;

    if (!arg)
        return NULL;

    int checkback_id = motor->movement.checkback_id;

    // Request current device latency
    struct motor_query q = { .query = MCLATENCYRX };
    // TODO: Something with the return value from the driver
    INVOKE(motor, read, &q);
    int latency = q.value.number;

    // Request a status update from the motor
    q.query = MCSTATUS;
    // TODO: Something with the return value from the driver
    INVOKE(motor, read, &q);
    struct motion_update * motion = &q.value.status;
    
    // Detect motion cancel
    if (motor->movement.checkback_id != checkback_id)
        // The move we're checking on here was canceled
        return NULL;

    bool completed = true;
    if (motion->vel_known && motion->velocity != 0) {
        /*
         * (perhaps it's safe to) Assume that the unit is decelerating.
         * Currently, the velocity of the unit is given. The unit will
         * continue to decelerate at a constant rate of D which we have in
         * the unit's profile.
         *
         * Find the intersection of the decel curve with the current
         * velocity:
         *   v
         *   ^
         *   |     ------------\
         *   |  - - - - - - - - X
         *   |                  |\
         *   +------------------+-x----------> t
         *                  Now | L Resting time
         *
         * The change in time is the base of the triangle. The slope of the
         * decel line is the unit's decel value, which will also equal the
         * rise over the run or v / dt. Therefore, dt = v / D, if D is
         * considered a positive value.
         */
        double dt = 1. * abs(motion->velocity) / motor->profile.decel.value;
        struct timespec callback = {
            .tv_sec = (int)dt,
            .tv_nsec = (int)1e9 * (dt - (int)dt)
        };
        if (callback.tv_sec == 0 && callback.tv_nsec < latency) {
            // For all intents and purposes, the unit is stopped, because we
            // won't be able to communicate with it again until well after
            // it stops.  Estimate the resting position of the unit and move
            // on.  dt is units of seconds and vel is units of steps per
            // second.  The area under the above triangle will be the
            // distance travelled by the unit, which is 1/2 * b * h i
            // or 1/2 * dt * v
            motor->position = motion->position + dt * (motion->velocity >> 1);
            motor->pos_known = true;
        }
        else {
            mcTraceF(50, LIB_CHANNEL, "Early. Callback in %dns",
		        callback.tv_nsec);
            // Subtract time for device latency. Since the data we currently
            // have is 1/2 latency old and the data we'll retrieve will be
            // 1/2 after we ask, start the entire latency time early
            callback.tv_nsec -= latency;
            while (callback.tv_nsec < 0) {
                callback.tv_sec--;
                callback.tv_nsec += (int)1e9;
            }
            // Call this routine again at the estimated time of completion
            // (minus about half of the unit's latency).
            motor->movement.checkback_id = mcCallback(&callback,
                mcMoveTrajectCompletionCheckback, motor);
            completed = false;
        }
    }
    else if (motion->pos_known) {
        motor->position = motion->position;
        motor->pos_known = true;
    }

    // Signal motion-completion event
    if (completed) {
        union event_data data = { .motion = *motion };
        struct event_info info = {
            .event = EV_MOTION,
            .data = &data,
        };
        mcSignalEvent(motor->driver, &info);
        mcTraceF(50, LIB_CHANNEL, "Signalling EV_MOTION event");
        motor->movement.checkback_id = 0;
    }

    return NULL;
}
