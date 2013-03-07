#include "../motor.h"

#include "motion.h"

#include "motor.h"
#include "event-motion.h"
#include "locks.h"
#include "message.h"
#include "client.h"

#include <errno.h>
#include <math.h>

static struct conversion {
    unit_type_t source;
    unit_type_t dest;
    double scale;
} factors[] = {
    { MILLI_INCH, INCH, 0.001 },
    { MILLI_INCH, MILLI_METER, 0.0254 },
    { MILLI_INCH, METER, 0.0000254 },

    { INCH, MILLI_INCH, 1000 },
    { INCH, MILLI_METER, 25.4 },
    { INCH, METER, 0.0254 },

    { MILLI_METER, MILLI_INCH, 39.370 },
    { MILLI_METER, INCH, 0.03937 },
    { MILLI_METER, METER, 0.001 },

    { MILLI_ROTATION, MILLI_RADIAN, 1.0 / (M_PI * 2) },
    { MILLI_ROTATION, MILLI_DEGREE, 360.0 },
    { MILLI_RADIAN, MILLI_ROTATION, 2.0 * M_PI },
    { MILLI_RADIAN, MILLI_DEGREE, 180.0 / M_PI },
    { MILLI_DEGREE, MILLI_ROTATION, 0.00278 },
    { MILLI_DEGREE, MILLI_RADIAN, M_PI / 180.0 },

    { MILLI_G, INCH_SEC2, 0.386088 },
    { MILLI_G, MILLI_INCH_SEC2, 386.088 },
    { MILLI_G, METER_SEC2, 0.009807 },
    { MILLI_G, MILLI_METER_SEC2, 9.807 },

    { 0, 0, 0.0 }
};

int
_move_check_and_get_revs(Motor * motor, int amount,
        unit_type_t units, motion_instruction_t * command) {
    // Convert the request distance into microrevolutions using the
    // configured units and scale of the motor. If this has not yet been
    // configured for the motor, the status of mcDistance... will reflect
    // the appropriate error code.
    int status;
    if (units == 0)
        status = mcDistanceToMicroRevs(motor, amount, &command->amount);
    else
        status = mcDistanceUnitsToMicroRevs(motor, amount, units,
            &command->amount);
    if (status != 0)
        return status;

    // Ensure the motor is safe to travel
    if (mcDriverIsLocked(motor->driver))
        return EBUSY;

    // Ensure the driver supports a move operation
    if (!SUPPORTED(motor, move))
        return ENOTSUP;

    // Ensure motor motion profile is available, check reflected profile on
    // the motor itself. Sync motor profile from the driver if necessary
    if (!motor->profile.attrs.loaded) {
        struct motor_query q = {
            .query = MCPROFILE,
            .value.profile = &motor->profile
        };
        INVOKE(motor, read, &q);
        motor->profile.attrs.loaded = true;
    }
    command->profile = motor->profile;

    if (!motor->pos_known) {
        struct motor_query q = { .query = MCPOSITION };
        if (INVOKE(motor, read, &q) == 0) {
            motor->position = q.value.number;
            motor->pos_known = true;
        }
    }

    // Handle motion completion checkback -- signal EV_MOTION when the move
    // is completed
    mcMoveTrajectCompletionCancel(motor);

    // Track initial details of the motor motion details.
    // NOTE: that mcMoveTrajectCompletion() expects the motor->movement to
    // be filled out in advance
    motor->movement = (struct motor_motion_details) {
        .pstart = motor->position,
        .type = command->type,
        .urevs = command->amount,
    };
    clock_gettime(CLOCK_REALTIME, &motor->movement.start);

    if (command->type != MCSLEW)
        mcMoveTrajectCompletion(motor);

    // TODO: Add tracing

    return 0;
}

int PROXYIMPL(mcMoveAbsolute, MOTOR motor, int distance) {
    motion_instruction_t command;
    command.type = MCABSOLUTE;

    int status = _move_check_and_get_revs(CONTEXT->motor, distance,
        0, &command);
    if (status != 0)
        return status;

    // Cancel the absolute move if already in position. In terms of event
    // triggering, the EV_MOTION event is already fired in
    // _move_check_and_get_revs if the targeted position is 0 distance away
    if (CONTEXT->motor->pos_known && (
            abs(CONTEXT->motor->position - command.amount)
            <= CONTEXT->motor->profile.accuracy.value))
        return 0;

    CONTEXT->motor->pos_known = false;
    return INVOKE(CONTEXT->motor, move, &command);
}

int
PROXYIMPL(mcMoveAbsoluteUnits, MOTOR motor, int distance,
        unit_type_t units) {

    motion_instruction_t command;
    command.type = MCABSOLUTE;

    int status = _move_check_and_get_revs(CONTEXT->motor, distance,
        units, &command);
    if (status != 0)
        return status;

    // Cancel the absolute move if already in position. In terms of event
    // triggering, the EV_MOTION event is already fired in
    // _move_check_and_get_revs if the targeted position is 0 distance away
    if (CONTEXT->motor->pos_known && (
            abs(CONTEXT->motor->position - command.amount)
            <= CONTEXT->motor->profile.accuracy.value))
        return 0;

    CONTEXT->motor->pos_known = false;
    return INVOKE(CONTEXT->motor, move, &command);
}

int
PROXYIMPL(mcMoveRelative, MOTOR motor, int distance) {

    motion_instruction_t command;
    command.type = MCRELATIVE;

    int status = _move_check_and_get_revs(CONTEXT->motor, distance, 0, &command);
    if (status != 0)
        return status;

    CONTEXT->motor->pos_known = false;
    return INVOKE(CONTEXT->motor, move, &command);
}

int
PROXYIMPL(mcMoveRelativeUnits, MOTOR motor, int distance,
        unit_type_t units) {

    motion_instruction_t command;
    command.type = MCRELATIVE;

    int status = _move_check_and_get_revs(CONTEXT->motor, distance, units,
        &command);
    if (status != 0)
        return status;

    CONTEXT->motor->pos_known = false;
    return INVOKE(CONTEXT->motor, move, &command);
}

/**
 * mcStop
 *
 * Commands the motor to stop immediately. Invokes DriverClass::stop
 *
 * Returns:
 * (int) 0 upon success
 * ENOTSUP - if motor driver does not support ::stop
 * EINVAL - if motor specified is invalid
 */
int
PROXYIMPL(mcStop, MOTOR motor) {

    // Ensure the driver supports a stop operation
    if (!SUPPORTED(CONTEXT->motor, stop))
        return ENOTSUP;

    mcMoveTrajectCompletionCancel(CONTEXT->motor);

    return INVOKE(CONTEXT->motor, stop, MCSTOP);
}

int
PROXYIMPL(mcHalt, MOTOR motor) {

    // Ensure the driver supports a stop operation
    if (!SUPPORTED(CONTEXT->motor, stop))
        return ENOTSUP;

    mcMoveTrajectCompletionCancel(CONTEXT->motor);

    return INVOKE(CONTEXT->motor, stop, MCHALT);
}

int
PROXYIMPL(mcEStop, MOTOR motor) {

    // Ensure the driver supports a stop operation
    if (!SUPPORTED(CONTEXT->motor, stop))
        return ENOTSUP;

    mcMoveTrajectCompletionCancel(CONTEXT->motor);

    return INVOKE(CONTEXT->motor, stop, MCESTOP);
}

int
PROXYIMPL(mcSlew, MOTOR motor, int rate) {

    motion_instruction_t command = { .type = MCSLEW };

    int status = _move_check_and_get_revs(CONTEXT->motor, rate, 0,
        &command);
    if (status != 0)
        return status;

    CONTEXT->motor->pos_known = false;
    return INVOKE(CONTEXT->motor, move, &command);
}

int
PROXYIMPL(mcSlewUnits, MOTOR motor, int rate, unit_type_t units) {

    motion_instruction_t command = { .type = MCSLEW };

    int status = _move_check_and_get_revs(CONTEXT->motor, rate, units, &command);
    if (status != 0)
        return status;

    CONTEXT->motor->pos_known = false;
    return INVOKE(CONTEXT->motor, move, &command);
}

int
PROXYIMPL(mcHome, MOTOR motor, enum home_type type,
        enum home_direction direction) {

    if (!SUPPORTED(CONTEXT->motor, home))
        return ENOTSUP;

    CONTEXT->motor->pos_known = false;
    return INVOKE(CONTEXT->motor, home, type, direction);
}

int
PROXYIMPL(mcUnitScaleSet, MOTOR motor, unit_type_t units, long long urevs) {
    
    if (units <= DEFAULT_UNITS)
        return ER_BAD_UNITS;

    CONTEXT->motor->op_profile.units = units;
    CONTEXT->motor->op_profile.scale = urevs;

    return 0;
}

int
PROXYIMPL(mcUnitScaleGet, MOTOR motor, OUT unit_type_t * units,
        OUT long long * urevs) {
    
    *units = CONTEXT->motor->op_profile.units;
    *urevs = CONTEXT->motor->op_profile.scale;

    return 0;
}

int
mcMicroRevsToDistance(Motor * motor, long long mrevs, int * distance) {
    // Lookup scale and distance
    if (motor->op_profile.units == 0)
        return ER_NO_UNITS;

    return mcMicroRevsToDistanceUnits(motor, mrevs, distance,
        motor->op_profile.units);
}

int
mcMicroRevsToDistanceUnitsF(Motor * motor, long long urevs,
        double * distance, unit_type_t units) {

    if (units == MICRO_REVS) {
        *distance = urevs;
        return 0;
    }
    else if (motor->op_profile.scale == 0)
        return ER_NO_SCALE;

    // Keep *distance value at a 1e6x scale until after the final
    // conversion to maintain accuracy
    // scale = rev / unit .: distance = urevs / scale
    *distance = (double) urevs / motor->op_profile.scale;
    
    if (units != motor->op_profile.units) {
        struct conversion * c;
        for (c = factors; c->source; c++)
            if (c->dest == units && c->source == motor->op_profile.units)
                break;
        if (c->source == 0)
            return ER_CANT_CONVERT;

        // The dest and source values are swapped from
        // mcDistanceUnitsToMicroRevs, because this is converting from
        // device units to requested units instead of visa versa
        *distance *= c->scale;
    }

    return 0;
}

int
mcMicroRevsToDistanceF(Motor * motor, long long mrevs, double * distance) {
    // Lookup scale and distance
    if (motor->op_profile.units == 0)
        return ER_NO_UNITS;

    return mcMicroRevsToDistanceUnitsF(motor, mrevs, distance,
        motor->op_profile.units);
}

int
mcMicroRevsToDistanceUnits(Motor * motor, long long urevs, int * distance,
        unit_type_t units) {
    double _distance;
    int status = mcMicroRevsToDistanceF(motor, urevs, &_distance);
    if (status)
        return status;

    *distance = (int) _distance;
    return 0;
}

int
mcDistanceToMicroRevs(Motor * motor, int distance, long long * urevs) {
    // Lookup scale and distance
    if (motor->op_profile.units == 0)
        return ER_NO_UNITS;

    return mcDistanceUnitsToMicroRevs(motor, distance,
        motor->op_profile.units, urevs);
}

int
mcDistanceUnitsToMicroRevs(Motor * motor, int distance, unit_type_t units,
        long long * urevs) {

    // Handle absolute units, such as urevs
    if (units == MICRO_REVS) {
        *urevs = distance;
        return 0;
    }
    else if (motor->op_profile.scale == 0)
        return ER_NO_SCALE;

    // scale = rev / unit .: revs = distance * units * scale
    *urevs = (long long) distance * (long long) 1e6;

    if (units != motor->op_profile.units) {
        struct conversion * c;
        for (c = factors; c->source; c++)
            if (c->source == units && c->dest == motor->op_profile.units)
                break;
        if (c->source == 0)
            return ER_CANT_CONVERT;

        // distance is configured in source units, motor is configured in
        // dest units. Therefore, if the motor is in mm and units are
        // specified in mils, then the value for distance will have to get
        // smaller -- so multiply by the factor.
        *urevs *= c->scale;
    }
    *urevs = (*urevs * motor->op_profile.scale) / (long long) 1e6;

    return 0;
}

int
PROXYIMPL(mcUnitToMicroRevs, MOTOR motor, unit_type_t unit,
        OUT long long * urevs) {
    return mcDistanceUnitsToMicroRevs(CONTEXT->motor, 1, unit, urevs);
}
