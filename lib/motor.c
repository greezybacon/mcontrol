#include "../motor.h"
#include "../drivers/driver.h"
#include "message.h"
#include "motor.h"
#include "client.h"

#include "driver.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <signal.h>

static const int MAX_ACTIVE_MOTOR_CONNECTIONS = 64;
static int motor_conn_count = 0;
static struct backend_motor * motor_list = NULL;
static int motor_uid = 1;

static void
mcGarbageCollect(void) {
    // Performs various cleanup of internally static lists
    struct backend_motor * m = motor_list;
    if (m != NULL) {
        while (m->id) {
            if (m->active && kill(m->client_pid, 0) == -1) {
                m->active = false;
                motor_conn_count--;
            }
            m++;
        }
    }
}

/**
 *  mcConnect
 *
 *  Finds the requested motor and connects to its queue. It also creates a
 *  return traffic queue for responses.
 */
PROXYIMPL(mcConnect, String connection, OUT MOTOR motor_t motor,
        bool recovery) {
    UNPACK_ARGS(mcConnect, args);

    if (motor_list == NULL)
        motor_list = calloc(MAX_ACTIVE_MOTOR_CONNECTIONS+1,
            sizeof *motor_list);

    // XXX: Do this pseudo randomly, or when all the connections are used up
    mcGarbageCollect();

    if (motor_conn_count == MAX_ACTIVE_MOTOR_CONNECTIONS)
        RETURN(ER_TOO_MANY);

    // Find first inactive motor connection entry
    struct backend_motor * m = motor_list;
    while (m->active) m++;

    // New motor connection
    // Configure server-side motor
    *m = (struct backend_motor) {
        .client_pid = message->pid,
        .id = motor_uid++
    };
    int status = mcDriverConnect(args->connection.buffer, &m->driver);
    if (status != 0 && !args->recovery)
        RETURN(status);

    if (m->driver == NULL)
        // Invalid connection string
        RETURN(EINVAL);

    m->active = true;
    motor_conn_count++;

    args->motor = m->id;
    
    RETURN(0);
}

PROXYIMPL(mcSearch, String driver, OUT String results) {
    UNPACK_ARGS(mcSearch, args);

    DriverClass * class = mcDriverLookup(args->driver.buffer);
    if (class == NULL)
        RETURN( EINVAL );

    RETURN( mcDriverSearch(class, args->results.buffer, args->results.size) );
}

Motor *
find_motor_by_id(motor_t id, int pid) {
    struct backend_motor * m = motor_list;
    // Scan through high-water-mark using the id member
    if (m != NULL) {
        while (m->id) {
            if (m->id == id && pid == m->client_pid)
                return m;
            m++;
        }
    }
    return NULL;
}

/**
 * mcDisconnect
 *
 * Server-side utility to mark a client motor connection as inactive. The
 * slot will be reused when another client connects using mcConnect()
 */
void mcDisconnect(motor_t motor) {
    if (motor_list == NULL)
        return;

    struct backend_motor * m = motor_list;
    while (m->id) {
        if (m->id == motor) {
            m->active = false;
            motor_conn_count--;
            break;
        }
        m++;
    }
}

int
mcMotorsForDriver(Driver * driver, Motor * list, int size) {
    int count=0;
    struct backend_motor * m = motor_list;

    // Scan through high-water-mark using the id member
    while (m->id) {
        if (m->driver == driver && m->active) {
            if (size < sizeof *m)
                break;
            *list = *m;
            size -= sizeof *m;
            count++;
        }
        m++;
    }
    return count;
}
