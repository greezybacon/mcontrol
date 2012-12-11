#include "../motor.h"

#include "profile.h"

#include "client.h"
#include "motor.h"

#include <errno.h>

PROXYIMPL(mcProfileGet, OUT Profile profile) {
    UNPACK_ARGS(mcProfileGet, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    args->profile = m->profile;

    RETURN(0);
}

PROXYIMPL(mcProfileSet, Profile profile) {
    UNPACK_ARGS(mcProfileSet, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    m->profile = args->profile;

    // Convert any embedded units in the profile to standard units of micro
    // revolutions
    long long T;
    if (m->profile.accel.measure.units) {
        mcDistanceUnitsToMicroRevs(m, m->profile.accel.measure.value,
            m->profile.accel.measure.units, &T);
        m->profile.accel.raw = T;
    }
    if (m->profile.decel.measure.units) {
        mcDistanceUnitsToMicroRevs(m, m->profile.decel.measure.value,
            m->profile.decel.measure.units, &T);
        m->profile.decel.raw = T;
    }
    if (m->profile.vmax.measure.units) {
        mcDistanceUnitsToMicroRevs(m, m->profile.vmax.measure.value,
            m->profile.vmax.measure.units, &T);
        m->profile.vmax.raw = T;
    }
    if (m->profile.vstart.measure.units) {
        mcDistanceUnitsToMicroRevs(m, m->profile.vstart.measure.value,
            m->profile.vstart.measure.units, &T);
        m->profile.vstart.raw = T;
    }
    if (m->profile.slip_max.measure.units) {
        mcDistanceUnitsToMicroRevs(m, m->profile.slip_max.measure.value,
            m->profile.slip_max.measure.units, &T);
        m->profile.slip_max.raw = T;
    }

    RETURN(0);
}

/*
 * profile.accel = (struct measurement) { .value = 200, .units = MILLI_G }
 */
PROXYIMPL(mcProfileGetAccel, Profile profile, unit_type_t units,
        OUT int value) {
    UNPACK_ARGS(mcProfileGetAccel, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    mcMicroRevsToDistanceUnits(m, m->profile.accel.raw, &args->value,
        args->units);

    RETURN(0);
}

PROXYIMPL(mcProfileGetDecel, Profile profile, unit_type_t units,
        OUT int value) {
    UNPACK_ARGS(mcProfileGetDecel, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    mcMicroRevsToDistanceUnits(m, m->profile.decel.raw, &args->value,
        args->units);

    RETURN(0);
}

PROXYIMPL(mcProfileGetInitialV, Profile profile, unit_type_t units,
        OUT int value) {
    UNPACK_ARGS(mcProfileGetInitialV, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    mcMicroRevsToDistanceUnits(m, m->profile.vstart.raw, &args->value,
        args->units);

    RETURN(0);
}

PROXYIMPL(mcProfileGetMaxV, Profile profile, unit_type_t units,
        OUT int value) {
    UNPACK_ARGS(mcProfileGetMaxV, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    mcMicroRevsToDistanceUnits(m, m->profile.vmax.raw, &args->value,
        args->units);

    RETURN(0);
}

PROXYIMPL(mcProfileGetDeadband, Profile profile, unit_type_t units,
        OUT int value) {
    UNPACK_ARGS(mcProfileGetDeadband, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    mcMicroRevsToDistanceUnits(m, m->profile.deadband.raw, &args->value,
        args->units);

    RETURN(0);
}

PROXYIMPL(mcProfileGetMaxSlip, Profile profile, unit_type_t units,
        OUT int value) {
    UNPACK_ARGS(mcProfileGetMaxSlip, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    mcMicroRevsToDistanceUnits(m, m->profile.slip_max.raw, &args->value,
        args->units);

    RETURN(0);
}
