#ifndef CONNECT_H
#define CONNECT_H

#include "driver.h"

// Server's view of a motor, which will keep information about motors
// separate from each client's view of the motor.
typedef struct backend_motor Motor;
struct backend_motor {
    int         id;             // Client motor id
    int         client_pid;
    bool        active;

    Driver *    driver;         // Driver controlling the motor

    Profile     profile;        // Client's requested motion profile
    OpProfile   op_profile;     // Client's requested operating guidelines

    unsigned short subscriptions[EV__LAST];  // Bitmask of subscribed events
};

extern void mcInitialize(void);
extern void mcGoodBye(void);
extern void mcDisconnect(motor_t motor);

extern Motor * find_motor_by_id(motor_t id, int pid);
extern int mcMotorsForDriver(Driver *, Motor *, int);

// Suppress auto motor argument
SLOW PROXYDEF(mcConnect, int, String * connection, OUT MOTOR motor_t * motor,
    bool recovery);
PROXYDEF(mcSearch, int, String * driver, OUT String * results);
#endif
