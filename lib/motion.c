#include "../motor.h"

#include "motion.h"

#include "motor.h"
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
    if (motor->driver->class->move == NULL)
        return ENOTSUP;

    // Ensure motor motion profile is available, check reflected profile on
    // the motor itself
    command->profile = motor->profile;

    // TODO: Add tracing

    return 0;
}

PROXYIMPL(mcMoveAbsolute, int distance) {
    UNPACK_ARGS(mcMoveAbsolute, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    motion_instruction_t command;
    command.type = MCABSOLUTE;

    int status = _move_check_and_get_revs(m, args->distance, 0, &command);
    if (status != 0)
        RETURN( status );

    RETURN( m->driver->class->move(m->driver, &command) );
}

PROXYIMPL(mcMoveAbsoluteUnits, int distance, unit_type_t units) {
    UNPACK_ARGS(mcMoveAbsoluteUnits, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    motion_instruction_t command;
    command.type = MCABSOLUTE;

    int status = _move_check_and_get_revs(m, args->distance, args->units, &command);
    if (status != 0)
        RETURN( status );

    RETURN( m->driver->class->move(m->driver, &command) );
}

PROXYIMPL(mcMoveRelative, int distance) {
    UNPACK_ARGS(mcMoveRelative, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    motion_instruction_t command;
    command.type = MCRELATIVE;

    int status = _move_check_and_get_revs(m, args->distance, 0, &command);
    if (status != 0)
        RETURN( status );

    RETURN( m->driver->class->move(m->driver, &command) );
}

PROXYIMPL(mcMoveRelativeUnits, int distance, unit_type_t units) {
    UNPACK_ARGS(mcMoveRelativeUnits, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    motion_instruction_t command;
    command.type = MCRELATIVE;

    int status = _move_check_and_get_revs(m, args->distance, args->units, &command);
    if (status != 0)
        RETURN( status );

    RETURN( m->driver->class->move(m->driver, &command) );
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
PROXYIMPL(mcStop) {
    UNPACK_ARGS(mcStop, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    // Ensure the driver supports a stop operation
    if (m->driver->class->stop == NULL)
        RETURN( ENOTSUP );

    RETURN( m->driver->class->stop(m->driver, MCSTOP) );
}

PROXYIMPL(mcSlew, int rate) {
    UNPACK_ARGS(mcSlew, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    motion_instruction_t command = {
        .type = MCSLEW
    };

    int status = _move_check_and_get_revs(m, args->rate, 0, &command);
    if (status != 0)
        RETURN( status );

    RETURN( m->driver->class->move(m->driver, &command) );
}

PROXYIMPL(mcSlewUnits, int rate, unit_type_t units) {
    UNPACK_ARGS(mcSlewUnits, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    motion_instruction_t command = {
        .type = MCSLEW
    };

    int status = _move_check_and_get_revs(m, args->rate, args->units, &command);
    if (status != 0)
        RETURN( status );

    RETURN( m->driver->class->move(m->driver, &command) );
}

PROXYIMPL(mcHome, enum home_type type, enum home_direction direction) {
    UNPACK_ARGS(mcHome, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->home == NULL)
        RETURN( ENOTSUP );

    RETURN( m->driver->class->home(m->driver, args->type, args->direction) );
}

PROXYIMPL(mcUnitScaleSet, unit_type_t units, long long urevs) {
    UNPACK_ARGS(mcUnitScaleSet, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );
    
    if (args->units <= DEFAULT_UNITS)
        RETURN( ER_BAD_UNITS );

    m->op_profile.units = args->units;
    m->op_profile.scale = args->urevs;

    RETURN( 0 );
}

PROXYIMPL(mcUnitScaleGet, OUT unit_type_t * units, OUT int * urevs) {
    UNPACK_ARGS(mcUnitScaleGet, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );
    
    args->units = m->op_profile.units;
    args->urevs = m->op_profile.scale;

    RETURN( 0 );
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
mcMicroRevsToDistance(Motor * motor, long long mrevs, int * distance) {
    // Lookup scale and distance
    if (motor->op_profile.units == 0)
        return ER_NO_UNITS;

    return mcMicroRevsToDistanceUnits(motor, mrevs, distance,
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

PROXYIMPL(mcUnitToMicroRevs, unit_type_t unit, OUT long long urevs) {
    UNPACK_ARGS(mcUnitToMicroRevs, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    int status;
    status = mcDistanceUnitsToMicroRevs(m, 1, args->unit, &args->urevs);
    if (status)
        RETURN(status);

    RETURN(0);
}
