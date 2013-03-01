#ifndef MOTOR_H
#define MOTOR_H

#include <mqueue.h>
#include <stdbool.h>

typedef const char * axis_t;

typedef int motor_t;                // Id of motor kept by client

enum event_name {
    EV__FIRST = 1,
    EV_MOTION = EV__FIRST,
    EV_POSITION,                    // Position reached
    EV_INPUT,                       // Input changed

    EV_EXCEPTION,
    EV_OVERTEMP,

    EV_MOTOR_RESET,                 // Fired after motor is reset
    EV_MOTOR_PROBE,                 // Fired when motor is discovered

    EV_TRACE,                       // Trace emitted through mcTrace et. all.

    EV__LAST
};

typedef enum event_name event_t;

typedef struct event_info event_info_t;
typedef void (*event_cb_t)(event_info_t *evt);

struct motion_update {
    bool        completed;      // Move finished normally
    bool        stalled;        // Move interrupted by stall
    bool        cancelled;      // Move replaced with another move
    bool        stopped;        // Stop was issued
    bool        failed;         // Unable to arrive at dest
    bool        in_progress;    // Still moving (progress update)
    bool        pos_known;      // Position element is valid
    bool        vel_known;      // Velocity field is valid
    unsigned char tries;        // Number of tries if completed
                                // successfully after retrying
    unsigned    error;          // urevs of slip
    long long   position;       // Current position (urevs), if known
    unsigned    velocity;       // Current velocity (urevs/s), if known
};

typedef union event_data event_data_t;
union event_data {                  // Data associated with the event
    long long   number;
    char        string[256];

    // EV_MOTION event payload
    struct motion_update motion;

    // EV_OVERTEMP event payload
    struct {
        // Current temperature, scale is dependent on the motor
        float       temperature;
        // If set, indicates that the motor is now disabled and will fail to
        // execute any further move commands
        bool        shutdown;
    } temp;

    struct {
        char    level;
        char    channel;
        char    buffer[254];
    } trace;
};

struct event_info {
    event_t     event;
    motor_t     motor;

    union event_data * data;
    void *      user;
};

enum motion_increment {
    DEFAULT_UNITS = 1,
    // Raw units (no conversion necessary)
    MICRO_REVS,

    MILLI_INCH,
    INCH,
    MILLI_METER,
    METER,
    MILLI_ROTATION,
    MILLI_RADIAN,
    MILLI_DEGREE,

    // Velocity types
    MILLI_INCH_SEC = MILLI_INCH,
    MILLI_METER_SEC = MILLI_METER,
    MILLI_ROTATION_SEC = MILLI_ROTATION,
    MILLI_RADIAN_SEC = MILLI_RADIAN,
    MILLI_DEGREE_SEC = MILLI_DEGREE,

    // Acceleration types
    MILLI_G = 20,
    INCH_SEC2 = INCH,
    MILLI_INCH_SEC2 = MILLI_INCH,
    METER_SEC2 = METER,
    MILLI_METER_SEC2 = MILLI_METER,
    MILLI_ROTATION_SEC2 = MILLI_ROTATION,
    MILLI_RADIAN_SEC2 = MILLI_RADIAN,
    MILLI_DEGREE_SEC2 = MILLI_DEGREE,
};
typedef enum motion_increment unit_type_t;

struct measurement {
    unsigned long long  value;
    unit_type_t         units;
};

typedef struct motion_profile Profile;
struct motion_profile {
    struct measurement  accel;      // mrev/s2
    struct measurement  decel;      // mrev/s2
    struct measurement  vmax;       // Max velocity (mrev/s)
    struct measurement  vstart;     // Initial velocity
    struct measurement  accuracy;   // Microrevs of accuracy for target of a move command

    // Properties honored by stepper motor units
    unsigned char       current_run; // Motor run current (%) XXX: Should this be in mA?
    unsigned char       current_hold; // Motor holding current (%)
    struct measurement  slip_max;    // Max difference between the encoder
                                     // indication of the unit and the
                                     // commanded position. This determines
                                     // when a stall is flagged.
    struct motion_profile_attrs {
        unsigned        hardware    :1;     // Profile saved in microcode / hardware
        unsigned        number      :3;     // Profile number (7 max, 1-based)

        // If set, the profile should be refreshed to/from the motor
        // (depends if the profile operation was get/set)
        unsigned        refresh     :1;     

        // Unset initially. The first time a get or set is requested of a
        // motor, the motor should be peeked/poked to retrieve the actual
        // values reflected in the device. Thereafter, the loaded flag
        // should be set to indicate that further changes are authoritative.
        unsigned        loaded      :1;
    } attrs;
};

typedef struct operating_profile OpProfile;
struct operating_profile {
    unit_type_t units;          // Units of measure used by this profile
    long long   scale;          // Number of mrevs in one unit

    bool        stop_if_stalled; // Halt immediately if stalled
    bool        maintain_position; // Autocorrect position if changes

    // Use the device's encoder. This setting may be auto-enabled by other
    // requested features that would require an encoder to be enabled
    bool        use_encoder;    

    // Turn coils off automatically after this many milliseconds.
    // < 0 to disable
    int         auto_off_delay; 
};

////////////// COMMANDS /////////////////////////
enum motor_query_type {
    MCPOSITION,
    MCVELOCITY,
    MCACCELERATING,
    MCMOVING,           // Device is currently moving
    MCSTALLED,
    MCDISABLED,         // Unit coils are offline
    MCINPUT,            // Query value of unit input -- specify number
    MCOUTPUT,           // Query value of unit ouput -- specity number
    MCBUSY,             // Device is executing microcode
    MCSTATUS,           // Retrive a (struct motion_update)

    // Individual items from motion profile
    MCPROFILE,          // Set and retrieve hardware profiles
    MCACCEL,
    MCDECEL,
    MCVMAX,
    MCVINITIAL,
    MCDEADBAND,
    MCRUNCURRENT,
    MCHOLDCURRENT,
    MCSLIPMAX,

    MCOPPROFILE,        // Set and retrieve operational profile

    // NOTE: Drivers may specify additional query types. Refer to individual
    // driver headers for specific query types supported by each respective
    // driver.
};
typedef enum motor_query_type motor_query_t;

#define DAEMON_QUEUE_NAME "/mcontrol"
#define CLIENT_QUEUE_FMT "/mcontrol-%d-%s"

enum move_type {
    MCABSOLUTE = 1,
    MCRELATIVE,
    MCSLEW,
    MCJITTER,
};
typedef enum move_type move_type_t;

// Types used by the stop driver entry
enum stop_type {
    MCSTOP = 1,     // Stop movement only
    MCHALT,         // Abort movement and microcode execution
    MCESTOP,        // Halt all accessible motors
};

// Types used by the home driver entry
enum home_type {
    MCHOMEDEF = 1,  // Use microcode-preferred homing method
    MCHOMESTOP,     // Home to hard stop
    MCHOMEMID,      // Home to midpoint (if supported)
    MCHOMENEAR,     // Home to near-side of home switch
    MCHOMEFAR,      // Home to far-side of home switch
};
enum home_direction {
    MCHOMERIGHT = 1, // Home in the positive direction
    MCHOMELEFT,     // Home in the negative direction
};

typedef struct motion_instruction motion_instruction_t;
struct motion_instruction {
    move_type_t     type;       // Relative, Absolute, Jitter, Slew
    long long       amount;     // Relative, Absolute move amount. Units here
                                // are micro-revolutions.
                                // Jitter: Maximum travel distance 
                                // Slew: Rate in micro-revs/sec
    int             count;      // Jitter: Number of times to repeat 
    Profile         profile;    // Profile to use for the motion instruction
};

enum error {
    ER_NO_DAEMON=1000,          // No one is listening
    ER_NO_UNITS,                // Units have not been configured for the motor
    ER_NO_SCALE,                // Scale has not been configured for the motor
    ER_BAD_UNITS,               // Operating profile units incorrect
    ER_CANT_CONVERT,            // Cant convert to requested units
    ER_TOO_MANY,                // Too many items in this list
    ER_BAD_CXN_STRING,          // Invalid connection string given
    ER_COMM_FAIL,               // Motor is not responding
    ER_BAD_FILE,                // Requested file does not exist
    ER_NOT_RESPONDING,          // Motor is not responding
    ER_DAEMON_BUSY,             // Daemon is not responding
};
typedef enum error error_type_t;

#endif
