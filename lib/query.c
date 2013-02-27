#include "../motor.h"
#include "../drivers/driver.h"

#include "message.h"
#include "query.h"
#include "dispatch.h"
#include "motor.h"
#include "client.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

int
PROXYIMPL (mcQueryInteger, MOTOR motor, motor_query_t query,
        OUT int * value) {

    if (!SUPPORTED(CONTEXT->motor, read))
        return ENOTSUP;

    struct motor_query q = { .query = query };

    INVOKE(CONTEXT->motor, read, &q);

    // Convert distance-based queries from microrevs
    switch (query) {
        case MCPOSITION:
        case MCVELOCITY:
            mcMicroRevsToDistance(CONTEXT->motor, q.value.number, value);
            break;
        default:
            *value = q.value.number;
    }

    return 0;
}

int
PROXYIMPL(mcQueryIntegerUnits, MOTOR motor, motor_query_t query,
        OUT int * value, unit_type_t units) {

    if (!SUPPORTED(CONTEXT->motor, read))
        return ENOTSUP;

    struct motor_query q = { .query = query };

    int status = INVOKE(CONTEXT->motor, read, &q);
    if (status)
        return status;

    // Convert distance-based queries from microrevs
    switch (query) {
        case MCPOSITION:
        case MCVELOCITY:
            mcMicroRevsToDistanceUnits(CONTEXT->motor, q.value.number,
                value, units);
            break;
        default:
            *value = q.value.number;
    }

    return 0;
}

int
PROXYIMPL (mcQueryIntegerWithStringItem, MOTOR motor, motor_query_t query,
        OUT int * value, String * item) {

    if (!SUPPORTED(CONTEXT->motor, read))
        return ENOTSUP;

    struct motor_query q = { .query = query };
    snprintf(q.arg.string, sizeof q.arg.string, "%s", item->buffer);

    int status = INVOKE(CONTEXT->motor, read, &q);
    if (status)
        return status;

    *value = q.value.number;
    return 0;
}

int
PROXYIMPL (mcQueryIntegerWithIntegerItem, MOTOR motor, motor_query_t query,
        OUT int * value, int item) {

    if (!SUPPORTED(CONTEXT->motor, read))
        return ENOTSUP;

    struct motor_query q = {
        .query = query,
        .arg.number = item
    };

    int status = INVOKE(CONTEXT->motor, read, &q);
    if (status)
        return status;

    *value = q.value.number;
    return 0;
}

int
PROXYIMPL (mcQueryFloat, MOTOR motor, motor_query_t query,
        OUT double * value) {

    if (!SUPPORTED(CONTEXT->motor, read))
        return ENOTSUP;

    struct motor_query q = { .query = query };

    int status = INVOKE(CONTEXT->motor, read, &q);
    if (status)
        return status;

    // Convert distance-based queries from microrevs
    switch (query) {
        case MCPOSITION:
        case MCVELOCITY:
            mcMicroRevsToDistanceF(CONTEXT->motor, q.value.number, value);
            break;
        default:
            *value = q.value.number;
    }

    return 0;
}

int
PROXYIMPL(mcQueryFloatUnits, MOTOR motor, motor_query_t query,
        OUT double * value, unit_type_t units) {

    if (!SUPPORTED(CONTEXT->motor, read))
        return ENOTSUP;

    struct motor_query q = { .query = query };

    int status = INVOKE(CONTEXT->motor, read, &q);
    if (status)
        return status;

    // Convert distance-based queries from microrevs
    switch (query) {
        case MCPOSITION:
        case MCVELOCITY:
            mcMicroRevsToDistanceUnitsF(CONTEXT->motor, q.value.number,
                value, units);
            break;
        default:
            *value = q.value.number;
    }

    return 0;
}

int
PROXYIMPL (mcQueryString, MOTOR motor, motor_query_t query,
        OUT String * value) {

    if (!SUPPORTED(CONTEXT->motor, read))
        return ENOTSUP;

    struct motor_query q = { .query = query };

    int status = INVOKE(CONTEXT->motor, read, &q);
    if (status)
        return status;

    if (q.value.string.size > 0)
        // Use min of status an sizeof args->value.buffer
        value->size = snprintf(value->buffer,
            sizeof value->buffer,
            "%s", q.value.string.buffer);

    return 0;
}

int
PROXYIMPL (mcPokeString, MOTOR motor, motor_query_t query, String * value) {

    if (!SUPPORTED(CONTEXT->motor, write))
        return ENOTSUP;

    struct motor_query q = { .query = query };

    q.value.string.size = snprintf(q.value.string.buffer,
        sizeof q.value.string.buffer,
        "%s", value->buffer);

    return INVOKE(CONTEXT->motor, write, &q);
}

int
PROXYIMPL (mcPokeInteger, MOTOR motor, motor_query_t query, int value) {

    if (!SUPPORTED(CONTEXT->motor, write))
        return ENOTSUP;

    struct motor_query q = { .query = query };

    // TODO: Convert POKE codes with units such as MCPOSITION
    switch (query) {
        case MCPOSITION:
            mcDistanceToMicroRevs(CONTEXT->motor, value, &q.value.number);
            break;
        default:
            q.value.number = value;
    }

    return INVOKE(CONTEXT->motor, write, &q);
}

int
PROXYIMPL (mcPokeIntegerUnits, MOTOR motor, motor_query_t query,
        int value, unit_type_t units) {

    if (!SUPPORTED(CONTEXT->motor, write))
        return ENOTSUP;

    struct motor_query q = { .query = query };

    // TODO: Convert POKE codes with units such as MCPOSITION
    switch (query) {
        case MCPOSITION:
            mcDistanceUnitsToMicroRevs(CONTEXT->motor, value, units,
                &q.value.number);
            break;
        default:
            q.value.number = value;
    }

    return INVOKE(CONTEXT->motor, write, &q);
}

int
PROXYIMPL (mcPokeIntegerWithStringItem, MOTOR motor, motor_query_t query,
        int value, String * item) {

    if (!SUPPORTED(CONTEXT->motor, write))
        return ENOTSUP;

    struct motor_query q = { .query = query };
    snprintf(q.arg.string, sizeof q.arg.string, "%s", item->buffer);

    q.value.number = value;

    return INVOKE(CONTEXT->motor, write, &q);
}

int
PROXYIMPL (mcPokeIntegerWithIntegerItem, MOTOR motor, motor_query_t query,
        int value, int item) {

    if (!SUPPORTED(CONTEXT->motor, write))
        return ENOTSUP;

    struct motor_query q = {
        .query = query,
        .arg.number = item,
    };
    q.value.number = value;

    return INVOKE(CONTEXT->motor, write, &q);
}

int
PROXYIMPL (mcPokeStringWithStringItem, MOTOR motor, motor_query_t query,
        String * value, String * item) {

    if (!SUPPORTED(CONTEXT->motor, write))
        return ENOTSUP;

    struct motor_query q = { .query = query };
    snprintf(q.arg.string, sizeof q.arg.string, "%s", item->buffer);
    q.value.string.size = snprintf(q.value.string.buffer,
        sizeof q.value.string.buffer,
        "%s", value->buffer);

    return INVOKE(CONTEXT->motor, write, &q);
}
