#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "../drivers/driver.h"
#include "message.h"
#include "locks.h"
#include "motor.h"
#include "client.h"

int
mcDriverLock(Driver * driver, int for_motor_id, lock_mode_t mode) {
    
    int * lock = driver->locks;
    while (*lock != 0) {
        if (*lock == for_motor_id) 
            // Driver is already locked for the requested motor.
            return 0;
        lock++;
    }
    // TODO: Perhaps detect max number of locks exceeded
    *lock = for_motor_id;
    
    return 0;
}

/**
 * mcMotorLock
 *
 * Requests the motor to be locked for the other, given motor. If the motor
 * to be locked is currently moving, it will continue unless estop is set.
 */
PROXYIMPL (mcMotorLock, int for_motor_id, lock_mode_t mode) {
    UNPACK_ARGS(mcMotorLock, args);

    Motor * m = find_motor_by_id(motor, message->pid);

    if (m == NULL)
        RETURN( EINVAL );
    else
        RETURN( mcDriverLock(m->driver, args->for_motor_id, args->mode) );
}

int
mcDriverUnlock(Driver * driver, int for_motor_id) {
    int * lock = driver->locks, * found_lock = NULL;
    while (*lock != 0) {
        if (*lock == for_motor_id) {
            // Motor does have a lock. 
            found_lock = lock;
        }
        lock++;
    }
    if (found_lock != NULL) {
        // Lock has advanced past the end of the list. Back it up to the
        // last entry, then move the last entry to the position of the
        // found_lock. Then null out the last entry
        *found_lock = *(--lock);
        *lock = 0;
        return 0;
    }
    return ENOENT;
}

PROXYIMPL (mcMotorUnlock, int for_motor_id) {
    UNPACK_ARGS(mcMotorUnlock, args);

    Motor * m = find_motor_by_id(motor, message->pid);

    if (m == NULL)
        RETURN(EINVAL);

    RETURN( mcDriverUnlock(m->driver, args->for_motor_id) );
}

bool
mcDriverIsLocked(Driver * driver) {
    return *driver->locks != 0;
}

PROXYIMPL (mcMotorIsLocked) {
    UNPACK_ARGS(mcMotorUnlock, args);

    Motor * m = find_motor_by_id(motor, message->pid);

    if (m == NULL)
        RETURN(EINVAL);

    RETURN( mcDriverIsLocked(m->driver) );
}
