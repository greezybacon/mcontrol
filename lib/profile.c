#include "../motor.h"

#include "profile.h"

#include "client.h"
#include "motor.h"

#include <errno.h>

int
PROXYIMPL(mcProfileGet, MOTOR motor, OUT Profile * profile) {
    if (!CONTEXT->motor->profile.attrs.loaded) {
        struct motor_query q = {
            .query = MCPROFILE,
            .value.profile = &CONTEXT->motor->profile
        };
        INVOKE(CONTEXT->motor, read, &q);
        CONTEXT->motor->profile.attrs.loaded = true;
    }
    *profile = CONTEXT->motor->profile;
    return 0;
}

int
PROXYIMPL(mcProfileSet, MOTOR motor, Profile * profile) {

    CONTEXT->motor->profile = *profile;

    // Convert any embedded units in the profile to standard units of micro
    // revolutions
    long long T;
    int status;
    Motor * m = CONTEXT->motor;
    if (m->profile.accel.value && m->profile.accel.units != MICRO_REVS) {
        status = mcDistanceUnitsToMicroRevs(m, m->profile.accel.value,
            m->profile.accel.units, &T);
        if (status)
            return status;
        m->profile.accel.value = T;
        m->profile.accel.units = MICRO_REVS;
    }
    if (m->profile.decel.value && m->profile.decel.units != MICRO_REVS) {
        status = mcDistanceUnitsToMicroRevs(m, m->profile.decel.value,
            m->profile.decel.units, &T);
        if (status)
            return status;
        m->profile.decel.value = T;
        m->profile.decel.units = MICRO_REVS;
    }
    if (m->profile.vmax.value && m->profile.vmax.units != MICRO_REVS) {
        status = mcDistanceUnitsToMicroRevs(m, m->profile.vmax.value,
            m->profile.vmax.units, &T);
        if (status)
            return status;
        m->profile.vmax.value = T;
        m->profile.vmax.units = MICRO_REVS;
    }
    if (m->profile.vstart.value && m->profile.vstart.units != MICRO_REVS) {
        status = mcDistanceUnitsToMicroRevs(m, m->profile.vstart.value,
            m->profile.vstart.units, &T);
        if (status)
            return status;
        m->profile.vstart.value = T;
        m->profile.vstart.units = MICRO_REVS;
    }
    if (m->profile.slip_max.value && m->profile.slip_max.units != MICRO_REVS) {
        status = mcDistanceUnitsToMicroRevs(m, m->profile.slip_max.value,
            m->profile.slip_max.units, &T);
        if (status)
            return status;
        m->profile.slip_max.value = T;
        m->profile.slip_max.units = MICRO_REVS;
    }
    if (m->profile.accuracy.value && m->profile.accuracy.units != MICRO_REVS) {
        status = mcDistanceUnitsToMicroRevs(m, m->profile.accuracy.value,
            m->profile.accuracy.units, &T);
        if (status)
            return status;
        m->profile.accuracy.value = T;
        m->profile.accuracy.units = MICRO_REVS;
    }

    return 0;
}

/*
 * profile.accel = (struct measurement) { .value = 200, .units = MILLI_G }
 */
int
PROXYIMPL(mcProfileGetAccel, MOTOR motor, Profile * profile, unit_type_t units,
        OUT int * value) {

    mcMicroRevsToDistanceUnits(CONTEXT->motor,
        CONTEXT->motor->profile.accel.value, value,
        units);

    return 0;
}

int
PROXYIMPL(mcProfileGetDecel, MOTOR motor, Profile * profile, unit_type_t units,
        OUT int * value) {

    mcMicroRevsToDistanceUnits(CONTEXT->motor,
        CONTEXT->motor->profile.decel.value, value,
        units);

    return 0;
}

int
PROXYIMPL(mcProfileGetInitialV, MOTOR motor, Profile * profile, unit_type_t units,
        OUT int * value) {

    mcMicroRevsToDistanceUnits(CONTEXT->motor,
        CONTEXT->motor->profile.vstart.value, value,
        units);

    return 0;
}

int
PROXYIMPL(mcProfileGetMaxV, MOTOR motor, Profile * profile, unit_type_t units,
        OUT int * value) {

    mcMicroRevsToDistanceUnits(CONTEXT->motor,
        CONTEXT->motor->profile.vmax.value, value,
        units);

    return 0;
}

int
PROXYIMPL(mcProfileGetDeadband, MOTOR motor, Profile * profile, unit_type_t units,
        OUT int * value) {

    mcMicroRevsToDistanceUnits(CONTEXT->motor,
        CONTEXT->motor->profile.accuracy.value, value,
        units);

    return 0;
}

int
PROXYIMPL(mcProfileGetMaxSlip, MOTOR motor, Profile * profile, unit_type_t units,
        OUT int * value) {

    mcMicroRevsToDistanceUnits(CONTEXT->motor,
        CONTEXT->motor->profile.slip_max.value, value,
        units);

    return 0;
}
