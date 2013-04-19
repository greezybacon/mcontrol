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
        struct {
            unsigned short  size;
            char            buffer[32];
        } string;                       // As a string
        Profile *   profile;            // Peek/Poke an entire profile
        OpProfile * operating_profile;  // Peek/Poke operational profile
        struct motion_update status;    // Peek motion status info
    } value;
};

typedef struct motor_driver_data Driver;

typedef int (*driver_event_callback_t)(Driver *, struct event_info *);

typedef struct motor_driver DriverClass;
struct motor_driver {
    char *                  name;
    char *                  description;
    char *                  revision;

    // Device initialization routines
    /**
     * search
     *
     * Scans device comm lines for possible motors (UPnP style) and finds
     * motors that can be possibly connected to. Each located motor's
     * connection string is placed into the given buffer separated by a NULL
     * character.
     *
     * Parameters:
     * cxns - (OUT char *) Character buffer to receive a list of connection
     *      strings
     * size - (int) Size of the character buffer
     *
     * Returns:
     * (int) number of connection strings placed in the buffer. Each
     * separated by a NULL character.
     */
    int (*search)(char *, int);
    /**
     * initialize
     *
     * Initializes a device referenced by the given connection string. The
     * received Driver * is the driver instance data that will be passed to
     * all other driver entry routines. The initialization routine can place
     * device-specific information in the ->internal member of the received
     * Driver * structure
     *
     * Parameters:
     * self - (Driver *) driver instance data
     * connection - (const char *) connection string of the device-to-be-
     *      connected
     *
     * Returns:
     * (int) 0 upon success, EINVAL for invalid connection string, ENOMEM if
     * unable to acquire memory for internal device information storage,
     * ER_COMM_FAIL if unable to communicate with the device.
     */
    int (*initialize)(Driver *, const char *);
    /**
     * destroy
     *
     * Formally disconnects from the device and cleans up anything
     * otherwise initialized in the ::initialize routine
     *
     * Parameters:
     * self - (Driver *) driver instance data
     */
    void (*destroy)(Driver *);
    /**
     * reset
     *
     * Reset the device, perhaps by a reboot. All unsaved settings should be
     * rolled back. Motion should stop and microcode execution should be
     * aborted.
     *
     * Parameters:
     * self - (Driver *) driver instance data
     */
    int (*reset)(Driver *);

    // Motion instructions
    /**
     * move
     *
     * Instructs the device to execute a move operation. Details of the move
     * operation are stored in the (struct motion_instruction) sent with the
     * request
     *
     * Parameters:
     * self - (Driver *) driver instance data
     * request - (struct motion_instruction *) move request details
     */
    int (*move)(Driver *, motion_instruction_t *);
    /**
     * stop
     *
     * Instructs the device to stop moving. How the device is to be stopped
     * is determined by the included (enum stop_type)
     *
     * Parameters:
     * self - (Driver *) driver instance data
     * how - (enum stop_type) how the device is to be stopped:
     *      MCSTOP - Stop the device by decelerating to a stop
     *      MCHALT - Stop as quickly as possible and abort running microcode
     *      MCESTOP - Stop as quickly as possible, abort running microcode
     *          and de-energize the motor coils. Also, if able to broadcast
     *          to other motors on the same comm channel, broadcast the
     *          ESTOP to all accessible motors.
     */
    int (*stop)(Driver *, enum stop_type);
    /**
     * home
     *
     * The driver should assume that the position of the motor is now
     * unknown and should home the device in the direction indicated using
     * the algorithm indicated. Homing will likely use microcode assistance
     * and is allowed to block if homing is implemented in the software
     * driver.
     *
     * Parameters:
     * self - (Driver *) driver instance data
     * type - (enum home_type) Algorithm to use for homing
     * dir - (enum home_direction) Intitial direction for homing.
     */
    int (*home)(Driver *, enum home_type, enum home_direction);

    // Queries
    /**
     * read, write
     *
     * Request arbitrary information from the device or device driver. This
     * is similar to a traditional ioctl call.
     *
     * Parameters:
     * self - (Driver *) driver instance data
     * query - (struct motor_query *) what to read from the device
     *      .query - (enum motor_query_type) what to peek/poke
     *      .value - (union) value to poke or to receive peek
     *      .arg - (union) item (if more than one) for peeking and poking.
     *          For instance, if reading a IO port, this would be the number
     *          of the port to read/write/configure.
     *
     * Returns:
     * (int) 0 upon success, EINVAL for invalid query, EIO if unable to
     * communicate with the device, ENOTSUP if the device or driver does not
     * support reading or writing the specified query.
     */
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
    int (*subscribe)(Driver *, event_t, driver_event_callback_t);
    int (*unsubscribe)(Driver *, event_t, driver_event_callback_t);
    // XXX: Pause/unpause events ?

    // Special
    /**
     * load_firmware, load_microcode
     *
     * Load specified firmware or microcode onto the device.
     *
     * Caveats:
     * This could be implemented as an write() call with MCFIRMWARE or
     * MCMICROCODE query at some point in the future.
     */
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
