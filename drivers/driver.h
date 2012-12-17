#ifndef DRIVER_H
#define DRIVER_H

#include "../motor.h"

#include <sys/types.h>

typedef struct motor_config_entry config_entry_t;
struct motor_config_entry {
    char        name[32];
    union {
        int     number;
        char    string[32];
    } value;
};

struct motor_query {
    motor_query_t   query;              // What to retrieve
    union {                             // Item/condition to receive
        long long   number;
        char        string[32];
    } arg;
    union {
        long long   number;             // As a number
        char        string[32];         // As a string
        Profile *   profile;            // Peek/Poke an entire profile
    };
};

typedef struct motor_driver_data Driver;

typedef int (*driver_event_callback_t)(Driver *, struct event_info *);

typedef struct motor_driver DriverClass;
struct motor_driver {
    char *                  name;
    char *                  description;
    char *                  revision;

    // Device initialization routines
    int (*search)(char *, int);             // Scan for devices -- return cxn str[]
    int (*initialize)(Driver *, const char *); // Initialize a device from cxn str
                                            // was returned from search()
    void (*destroy)(Driver *);

    // Motion instructions
    int (*move)(Driver *, motion_instruction_t *);
    int (*stop)(Driver *, enum stop_type);
    int (*reset)(Driver *);
    int (*home)(Driver *);

    // Queries
    int (*read)(Driver *, struct motor_query *);
    int (*write)(Driver *, struct motor_query *);

    // Events
    /**
     * notify
     *
     * Request notification by event of an event not normally triggered. For
     * instance, when a device reaches a certain location or when a move
     * operation is completed. These events are not normally transmitted
     * because they would be too noisy (move complete) or impossible to
     * predict (position reached). The driver may notify all registered
     * callbacks of the event or just the specified one.
     *
     * Notifications are only delivered once. Thereafter, the notify routine
     * will have to be called again to receive further notifications of the
     * same event.
     *
     * Call the notify method with the callback set to NULL to cancel a
     * notification request registered for the given event and argument.
     *
     * Arguments:
     * XXX: Use a structure for better future-proofing
     * event_t event - Event to be notified of
     * int argument - Argument / condition for notification
     * driver_event_callback_t callback - Notify this client
     */
    int (*notify)(Driver *, event_t, int, driver_event_callback_t);
    // XXX: Pause/unpause events ?
    int (*unsubscribe)(Driver *, driver_event_callback_t);

    // Special
    int (*load_firmware)(Driver *, const char *);
    int (*load_microcode)(Driver *, const char *);
}; 

struct motor_driver_data {
    char                    name[16];   // Axis the driver controls
    int                     id;         // Driver device id
    int                     group;      // Thread group driver belongs to.
                                        // Used to assist in keeping
                                        // requests for motors on different
                                        // comm ports non-blocking

    int                     locks[32];  // Locks held on the motor
                                        // (by motor-id)
    DriverClass *           class;

    void *                  internal;   // Specific driver data
};

#endif
