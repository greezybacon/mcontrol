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

PROXYIMPL (mcQueryInteger, motor_query_t query, OUT int value) {
    UNPACK_ARGS(mcQueryInteger, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->read == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = { .query = args->query };

    m->driver->class->read(m->driver, &q);

    // Convert distance-based queries from microrevs
    switch (args->query) {
        case MCPOSITION:
        case MCVELOCITY:
            mcMicroRevsToDistance(m, q.value.number, &args->value);
            break;
        default:
            args->value = q.value.number;
    }

    RETURN(0);
}

PROXYIMPL(mcQueryIntegerUnits, int, motor_query_t query, OUT int value,
        unit_type_t units) {
    UNPACK_ARGS(mcQueryIntegerUnits, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN(EINVAL);

    if (m->driver->class->read == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = { .query = args->query };

    m->driver->class->read(m->driver, &q);

    // Convert distance-based queries from microrevs
    switch (args->query) {
        case MCPOSITION:
        case MCVELOCITY:
            mcMicroRevsToDistanceUnits(m, q.value.number, &args->value,
                args->units);
            break;
        default:
            args->value = q.value.number;
    }

    RETURN(0);
}

PROXYIMPL (mcQueryIntegerWithStringItem, motor_query_t query, OUT int value,
        String item) {
    UNPACK_ARGS(mcQueryIntegerWithStringItem, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->read == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = { .query = args->query };
    snprintf(q.arg.string, sizeof q.arg.string, "%s", args->item.buffer);

    int status = m->driver->class->read(m->driver, &q);
    if (status)
        RETURN(status);

    args->value = q.value.number;
    RETURN(0);
}

PROXYIMPL (mcQueryIntegerWithIntegerItem, motor_query_t query, OUT int value,
        int item) {
    UNPACK_ARGS(mcQueryIntegerWithIntegerItem, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->read == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = {
        .query = args->query,
        .arg.number = args->item
    };

    int status = m->driver->class->read(m->driver, &q);
    if (status)
        RETURN(status);

    args->value = q.value.number;
    RETURN(0);
}

PROXYIMPL (mcQueryFloat, motor_query_t query, OUT double value) {
    UNPACK_ARGS(mcQueryFloat, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->read == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = { .query = args->query };

    int status = m->driver->class->read(m->driver, &q);
    if (status)
        RETURN(status);

    // Convert distance-based queries from microrevs
    switch (args->query) {
        case MCPOSITION:
        case MCVELOCITY:
            mcMicroRevsToDistanceF(m, q.value.number, &args->value);
            break;
        default:
            args->value = q.value.number;
    }

    RETURN(0);
}

PROXYIMPL(mcQueryFloatUnits, int, motor_query_t query, OUT int value,
        unit_type_t units) {
    UNPACK_ARGS(mcQueryFloatUnits, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN(EINVAL);

    if (m->driver->class->read == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = { .query = args->query };

    int status = m->driver->class->read(m->driver, &q);
    if (status)
        RETURN(status);

    // Convert distance-based queries from microrevs
    switch (args->query) {
        case MCPOSITION:
        case MCVELOCITY:
            mcMicroRevsToDistanceUnitsF(m, q.value.number, &args->value,
                args->units);
            break;
        default:
            args->value = q.value.number;
    }

    RETURN(0);
}

PROXYIMPL (mcQueryString, motor_query_t query, OUT String value) {
    UNPACK_ARGS(mcQueryString, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->read == NULL)
        RETURN( ENOTSUP );

    int size, status;
    struct motor_query q = { .query = args->query };

    status = m->driver->class->read(m->driver, &q);
    if (q.value.string.size > 0)
        // Use min of status an sizeof args->value.buffer
        args->value.size = snprintf(args->value.buffer,
            sizeof args->value.buffer,
            "%s", q.value.string.buffer);

    RETURN(status);
}

PROXYIMPL (mcPokeString, motor_query_t query, String value) {
    UNPACK_ARGS(mcPokeString, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->write == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = { .query = args->query };
    q.value.string.size = snprintf(q.value.string.buffer,
        sizeof q.value.string.buffer,
        "%s", args->value.buffer);

    RETURN( m->driver->class->write(m->driver, &q) );
}

PROXYIMPL (mcPokeInteger, motor_query_t query, int value) {
    UNPACK_ARGS(mcPokeInteger, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->write == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = {
        .query = args->query
    };

    // TODO: Convert POKE codes with units such as MCPOSITION
    switch (args->query) {
        case MCPOSITION:
            mcDistanceToMicroRevs(m, args->value, &q.value.number);
            break;
        default:
            q.value.number = args->value;
    }

    RETURN( m->driver->class->write(m->driver, &q) );
}

PROXYIMPL (mcPokeIntegerUnits, motor_query_t query, int value,
        unit_type_t units) {
    UNPACK_ARGS(mcPokeIntegerUnits, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->write == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = {
        .query = args->query
    };

    // TODO: Convert POKE codes with units such as MCPOSITION
    switch (args->query) {
        case MCPOSITION:
            mcDistanceUnitsToMicroRevs(m, args->value, args->units,
                &q.value.number);
            break;
        default:
            q.value.number = args->value;
    }

    RETURN( m->driver->class->write(m->driver, &q) );
}

PROXYIMPL (mcPokeIntegerWithStringItem, motor_query_t query, int value,
        String item) {
    UNPACK_ARGS(mcPokeIntegerWithStringItem, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->write == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = {
        .query = args->query
    };
    snprintf(q.arg.string, sizeof q.arg.string, "%s", args->item.buffer);

    q.value.number = args->value;

    RETURN( m->driver->class->write(m->driver, &q) );
}

PROXYIMPL (mcPokeIntegerWithIntegerItem, motor_query_t query, int value,
        int item) {
    UNPACK_ARGS(mcPokeIntegerWithIntegerItem, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->write == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = {
        .query = args->query,
        .arg.number = args->item,
    };
    q.value.number = args->value;

    RETURN( m->driver->class->write(m->driver, &q) );
}

PROXYIMPL (mcPokeStringWithStringItem, motor_query_t query, String value,
        String item) {
    UNPACK_ARGS(mcPokeStringWithStringItem, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->write == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = {
        .query = args->query
    };
    snprintf(q.arg.string, sizeof q.arg.string, "%s", args->item.buffer);
    q.value.string.size = snprintf(q.value.string.buffer,
        sizeof q.value.string.buffer,
        "%s", args->value.buffer);

    RETURN( m->driver->class->write(m->driver, &q) );
}
