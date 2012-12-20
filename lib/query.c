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
            mcMicroRevsToDistance(m, q.number, &args->value);
            break;
        default:
            args->value = q.number;
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
            mcMicroRevsToDistanceUnits(m, q.number, &args->value, args->units);
            break;
        default:
            args->value = q.number;
    }

    RETURN(0);
}

PROXYIMPL (mcQueryIntegerItem, motor_query_t query, OUT int value, String item) {
    UNPACK_ARGS(mcQueryIntegerItem, args);

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

    args->value = q.number;
    RETURN(0);
}

PROXYIMPL (mcQueryString, motor_query_t query, OUT String value) {
    UNPACK_ARGS(mcQueryString, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->read == NULL)
        RETURN( ENOTSUP );

    int size;
    struct motor_query q = { .query = args->query };

    size = m->driver->class->read(m->driver, &q);
    if (size > 0)
        // Use min of status an sizeof args->value.buffer
        args->value.size = snprintf(args->value.buffer,
            sizeof args->value.buffer, "%s", q.string);

    RETURN(0);
}

PROXYIMPL (mcPokeString, motor_query_t query, String value) {
    UNPACK_ARGS(mcPokeString, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->write == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = { .query = args->query };
    snprintf(q.string, sizeof q.string, "%s", args->value.buffer);

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
            mcDistanceToMicroRevs(m, args->value, &q.number);
            break;
        default:
            q.number = args->value;
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
            mcDistanceUnitsToMicroRevs(m, args->value, args->units, &q.number);
            break;
        default:
            q.number = args->value;
    }

    RETURN( m->driver->class->write(m->driver, &q) );
}

PROXYIMPL (mcPokeIntegerItem, motor_query_t query, int value, String item) {
    UNPACK_ARGS(mcPokeIntegerItem, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->write == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = {
        .query = args->query
    };
    snprintf(q.arg.string, sizeof q.arg.string, "%s", args->item.buffer);

    q.number = args->value;

    RETURN( m->driver->class->write(m->driver, &q) );
}

PROXYIMPL (mcPokeStringItem, motor_query_t query, String value, String item) {
    UNPACK_ARGS(mcPokeStringItem, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->write == NULL)
        RETURN( ENOTSUP );

    struct motor_query q = {
        .query = args->query
    };
    snprintf(q.arg.string, sizeof q.arg.string, "%s", args->item.buffer);
    snprintf(q.string, sizeof q.string, "%s", args->value.buffer);

    RETURN( m->driver->class->write(m->driver, &q) );
}
