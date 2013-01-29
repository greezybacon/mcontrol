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
    if (m->profile.accel.units != MICRO_REVS) {
        mcDistanceUnitsToMicroRevs(m, m->profile.accel.value,
            m->profile.accel.units, &T);
        m->profile.accel.value = T;
        m->profile.accel.units = MICRO_REVS;
    }
    if (m->profile.decel.units != MICRO_REVS) {
        mcDistanceUnitsToMicroRevs(m, m->profile.decel.value,
            m->profile.decel.units, &T);
        m->profile.decel.value = T;
        m->profile.decel.units = MICRO_REVS;
    }
    if (m->profile.vmax.units != MICRO_REVS) {
        mcDistanceUnitsToMicroRevs(m, m->profile.vmax.value,
            m->profile.vmax.units, &T);
        m->profile.vmax.value = T;
        m->profile.vmax.units = MICRO_REVS;
    }
    if (m->profile.vstart.units != MICRO_REVS) {
        mcDistanceUnitsToMicroRevs(m, m->profile.vstart.value,
            m->profile.vstart.units, &T);
        m->profile.vstart.value = T;
        m->profile.vstart.units = MICRO_REVS;
    }
    if (m->profile.slip_max.units != MICRO_REVS) {
        mcDistanceUnitsToMicroRevs(m, m->profile.slip_max.value,
            m->profile.slip_max.units, &T);
        m->profile.slip_max.value = T;
        m->profile.slip_max.units = MICRO_REVS;
    }
    if (m->profile.accuracy.units != MICRO_REVS) {
        mcDistanceUnitsToMicroRevs(m, m->profile.accuracy.value,
            m->profile.accuracy.units, &T);
        m->profile.accuracy.value = T;
        m->profile.accuracy.units = MICRO_REVS;
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

    mcMicroRevsToDistanceUnits(m, m->profile.accel.value, &args->value,
        args->units);

    RETURN(0);
}

PROXYIMPL(mcProfileGetDecel, Profile profile, unit_type_t units,
        OUT int value) {
    UNPACK_ARGS(mcProfileGetDecel, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    mcMicroRevsToDistanceUnits(m, m->profile.decel.value, &args->value,
        args->units);

    RETURN(0);
}

PROXYIMPL(mcProfileGetInitialV, Profile profile, unit_type_t units,
        OUT int value) {
    UNPACK_ARGS(mcProfileGetInitialV, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    mcMicroRevsToDistanceUnits(m, m->profile.vstart.value, &args->value,
        args->units);

    RETURN(0);
}

PROXYIMPL(mcProfileGetMaxV, Profile profile, unit_type_t units,
        OUT int value) {
    UNPACK_ARGS(mcProfileGetMaxV, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    mcMicroRevsToDistanceUnits(m, m->profile.vmax.value, &args->value,
        args->units);

    RETURN(0);
}

PROXYIMPL(mcProfileGetDeadband, Profile profile, unit_type_t units,
        OUT int value) {
    UNPACK_ARGS(mcProfileGetDeadband, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    mcMicroRevsToDistanceUnits(m, m->profile.accuracy.value, &args->value,
        args->units);

    RETURN(0);
}

PROXYIMPL(mcProfileGetMaxSlip, Profile profile, unit_type_t units,
        OUT int value) {
    UNPACK_ARGS(mcProfileGetMaxSlip, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    mcMicroRevsToDistanceUnits(m, m->profile.slip_max.value, &args->value,
        args->units);

    RETURN(0);
}
