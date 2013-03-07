#ifndef LIBMOTOR_H
#define LIBMOTOR_H

#include "../motor.h"
#include "driver.h"

struct motor_motion_details {
    // Starting information
    long long           pstart;         // Starting position (urevs)
    struct timespec     start;          // Absolute start time

    // Motion timing information
    unsigned            vmax_us;        // Estimated end of acceleration
                                        // ramp -- usecs rel to start
    unsigned            decel_us;       // Estimated start of decel ramp
                                        // -- usecs rel to start
    struct timespec     projected;      // Estimated time of completion (abs)

    // Target information
    enum move_type      type;           // Move type (MCABSOLUTE, etc)
    long long           urevs;          // Requested revolutions

    // Details filled in after completion of motion request
    struct timespec     completed;      // Actual time of completion
    unsigned            error;          // Following error (urevs)

    // Realtime/status information
    bool                moving;         // Stall event occured since start

    int                 checkback_id;   // Id from mcCallback call
};

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

    struct motor_motion_details movement; // Current/previous movement info

    bool        pos_known;      // If last-known position is known
    long long   position;       // Last-known position of the motor

    unsigned short subscriptions[EV__LAST];  // Bitmask of subscribed events
};

#endif
