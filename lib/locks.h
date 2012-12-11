#ifndef LOCKS_H
#define LOCKS_H

#include "driver.h"

enum lock_mode {
    LOCK_MOTION_COMPLETE=1,
    LOCK_MOTION_ESTOP,
    LOCK_MOTION_WAIT
};
typedef enum lock_mode lock_mode_t;

extern int mcDriverLock(Driver * driver, int for_motor_id, lock_mode_t mode);
extern int mcDriverUnlock(Driver * driver, int for_motor_id);
extern bool mcDriverIsLocked(Driver * driver);

PROXYDEF(mcMotorLock, int, int for_motor_id, lock_mode_t mode);
PROXYDEF(mcMotorUnlock, int, int for_motor_id);
PROXYDEF(mcMotorIsLocked, bool);

#endif
