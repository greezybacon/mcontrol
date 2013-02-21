#ifndef CONNECT_H
#define CONNECT_H

#include "motor.h"
#include "message.h"

extern void mcInitialize(void);
extern void mcGoodBye(void);
extern void mcInactivate(Motor * motor);

extern Motor * find_motor_by_id(motor_t id, int pid);
extern int mcMotorsForDriver(Driver *, Motor *, int);

// Suppress auto motor argument
SLOW PROXYSTUB(int, mcConnect, String * connection, OUT MOTOR * motor,
    bool recovery);

PROXYSTUB(int, mcDisconnect, MOTOR motor);
PROXYSTUB(int, mcSearch, String * driver, OUT String * results);

#endif
