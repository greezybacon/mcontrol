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
int
PROXYIMPL (mcMotorLock, MOTOR motor, int for_motor_id, lock_mode_t mode) {
    return mcDriverLock(CONTEXT->motor->driver, for_motor_id, mode);
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

int
PROXYIMPL (mcMotorUnlock, MOTOR motor, int for_motor_id) {
    return mcDriverUnlock(CONTEXT->motor->driver, for_motor_id);
}

bool
mcDriverIsLocked(Driver * driver) {
    return *driver->locks != 0;
}

bool
PROXYIMPL (mcMotorIsLocked, MOTOR motor) {
    return mcDriverIsLocked(CONTEXT->motor->driver);
}
