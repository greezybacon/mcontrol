#ifndef MOTOR_H
#define MOTOR_H

#include "../motor.h"
#include "driver.h"

// Server's view of a motor, which will keep information about motors
// separate from each client's view of the motor.
typedef struct backend_motor Motor;
struct backend_motor {
    int         id;             // Client motor id
    int         client_pid;
    bool        active;

    struct driver_instance *
                instance;       // Driver controlling the motor
    Driver *    driver;         // Shortcut to instance->driver

    Profile     profile;        // Client's requested motion profile
    OpProfile   op_profile;     // Client's requested operating guidelines

    unsigned short subscriptions[EV__LAST];  // Bitmask of subscribed events
};

#endif
