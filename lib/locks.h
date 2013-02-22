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

PROXYSTUB(int, mcMotorLock, MOTOR motor, int for_motor_id, lock_mode_t mode);
PROXYSTUB(int, mcMotorUnlock, MOTOR motor, int for_motor_id);
PROXYSTUB(bool, mcMotorIsLocked, MOTOR motor);

#endif
