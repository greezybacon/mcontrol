#ifndef MDRIVE
#define MDRIVE

#include "../driver.h"

#define _POSIX_C_SOURCE 200809L
#define _POSIX_SOURCE 1

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_PORT_SPEED 9600

// Maximum times event_subscribe can be called for various event handler
// routine subscriptions
#define MAX_SUBSCRIPTIONS 48

typedef struct mdrive_response mdrive_response_t;
struct mdrive_response {
    char            buffer[64];
    unsigned        txid;
    char            address;            // Address of unit emitting an event
    uint8_t         ack_location;       // Where the ack was found (for CK>0)
    unsigned        received        :7; // Total chars received from unit
    unsigned        length          :7; // Length of buffer
    unsigned        code            :8; // Error/event code indicated by the unit
    unsigned        ack             :1;
    unsigned        nack            :1;
    unsigned        checksum_good   :1; // Checksum of unit-sent data is correct
    unsigned        event           :1; // Response code is an event code
    unsigned        error           :1; // Response code is an error code
    unsigned        processed       :1; // Signaled when no more input is anticipated for this response
    unsigned        echo            :1; // Response included an echo from the unit
    unsigned        prompt          :1; // Response included a '>' char
    unsigned        crlf            :1; // CRLF received in response
    unsigned        in_error        :1; // Internal to response processor
};

enum mdrive_response_class {
    RESPONSE_OK=0,
    RESPONSE_RETRY,                     // Error 63
    RESPONSE_ERROR,                     // Error exists on unit (not sent)
    RESPONSE_NACK,                      // Request not processed by unit
    RESPONSE_BAD_CHECKSUM,              // Unit-sent checksum is incorrect
    RESPONSE_UNKNOWN,                   // Not properly classified by driver
    RESPONSE_TIMEOUT,                   // No response from the unit
    RESPONSE_IOERROR,                   // Unable to send data to the unit
};

typedef struct event_callback event_callback_t;
struct event_callback {
    bool                active;     // Slot unused if not set
    bool                paused;     // Used to pause event receives
    enum event_name     event;      // Subscribed event
    int                 condition;  // Condition of the event
    driver_event_callback_t callback;
};

typedef struct mdrive_stats mdrive_stats_t;
struct mdrive_stats {
    // Communication stats
    unsigned            rx;         // Packets
    unsigned            rxbytes;    // Bytes
    unsigned            tx;
    unsigned            txbytes;
    unsigned            acks;
    unsigned            nacks;
    unsigned            timeouts;   // No response from unit
    unsigned            resends;
    unsigned            bad_checksums;
    unsigned            overflows;  // Error 63's received when talking to this unit
    unsigned            waittime;   // Milliseconds spent waiting XXX: Useful?
    unsigned            latency;    // Average latency (nanoseconds) over last
                                    // 20 transmissions

    // Operational stats
    unsigned            stalls;
    unsigned            reboots;
    unsigned long long  movingtime; // Time (ms) in motion
    unsigned long long  idletime;   // Time (ms) between moves
    unsigned long long  offtime;    // Time (ms) with DE=0
};

struct motion_details {
    // Starting information
    int                 pstart;         // Starting position (steps)
    struct timespec     start;          // Absolute start time

    // Motion timing information
    long                vmax_us;        // Estimated end of acceleration
                                        // ramp -- usecs rel to start
    long                decel_us;       // Estimated start of decel ramp
                                        // -- usecs rel to start
    struct timespec     projected;      // Estimated time of completion (abs)

    // Target information
    enum move_type      type;           // Move type (MCABSOLUTE, etc)
    long long           urevs;          // Requested revolutions

    // Details filled in after completion of motion request
    struct timespec     completed;      // Actual time of completion
    int                 error;          // Following error (urevs)
    unsigned char       stalls;         // Number of stalls
};

#include "queue.h"

typedef struct mdrive_axis_device_list mdrive_axis_device_t;
struct mdrive_axis_device_list {
    pthread_mutex_t     txlock;
    pthread_mutex_t     rxlock;
    char                name[32];       // Name of device (serial port)
    int                 fd;
    int                 speed;
    int                 active_axes;    // Reference counting to detect when
                                        // connection can be freed
    //struct termios      termios;        // Saved terminal settings

    int                 txid;           // Current transactionid
    struct timespec     lasttx;         // Time of last transmission
    struct timespec     lastActivity;   // Time of last tx or rx
    pthread_cond_t      has_data;

    queue_t             queue;
    pthread_t           read_thread;

    mdrive_axis_device_t * next;
};

enum mdrive_io_type {
    IO_INPUT=0,
    IO_HOME,
    IO_LIMIT_POS,
    IO_LIMIT_NEG,
    IO_G0,
    IO_SOFT_STOP,
    IO_PAUSE,
    IO_JOG_POS,
    IO_JOG_NEG,
    IO_ANALOG_VOLTAGE,      // Only valid for S5/I5
    IO_ANALOG_CURRENT,      // Only valid for S5/I5
    IO_RESET,
    IO_OUTPUT=16,
    IO_MOVING,
    IO_FAULT,
    IO_STALL,
    IO_DELTA_V,
    IO_MOVING_ABS=23,
};

struct mdrive_io_config {
    enum mdrive_io_type type;
    bool                active_high;
    bool                source;
};

typedef struct mdrive_axis_list mdrive_axis_t;
struct mdrive_axis_list {
    mdrive_axis_device_t * device;
    char                serial_number[16];
    char                part_number[16];
    char                firmware_version[8]; // System firmware version

    bool                party_mode;     // Device is in party mode
    char                address;
    short               txnest;         // Used to allow recursion when
                                        // retrieving error codes from unit
    short               checksum;       // Comm checksum mode
    short               echo;           // Comm echo mode
    bool                upgrade_mode;   // Unit is in upgrade mode
    bool                ignore_errors;  // Don't auto-fetch error number

    int                 speed;          // Speed of this axis, which allows
                                        // axes to share a port and operate
                                        // at different speeds
    mdrive_stats_t      stats;          // Communication and performance stats

    // Motion information
    int                 steps_per_rev;  // Microsteps per revolution
    int                 encoder;        // Encoder enabled
    int                 position;       // Last known position
    Profile             profile;        // Current profile represented on the device
    struct motion_details movement;     // Information of last movement
    int                 cb_complete;    // Callback ID for completion event
    bool                drive_disabled; // DE=0

    event_callback_t    event_handlers[MAX_SUBSCRIPTIONS];

    // Items that were lazy loaded. Kept in a bitmask for easy resetting
    // when the unit is detected to have rebooted
    union {
        unsigned        mask;
        struct {
            unsigned    comm_config:1;  // EM,CK
            unsigned    profile:1;      // A,D,VI,VM,RC,HC
            unsigned    encoder:1;      // EE,MS
            unsigned    io:1;           // S#
            unsigned    enabled:1;      // DE
        };
    } loaded;

    // I/O configuration
    struct mdrive_io_config io1;
    struct mdrive_io_config io2;
    struct mdrive_io_config io3;
    struct mdrive_io_config io4;

    // Trip notification configuration on the unit
    union {
        unsigned        mask;
        struct {
            // LSB on little-endian machines
            unsigned    input:1;
            unsigned    abs_position:1;
            unsigned    capture:1;
            unsigned    time:1;
            unsigned    rel_position:1;
            unsigned    hybrid:1;
            unsigned    :0;
            // These are implemented in software
            unsigned    completion:1;
        };
    } trip;

    // Well-known labels (for homing, moving, etc)
    struct {
        char            home[3];
        char            move[3];
        char            jitter[3];
    } labels;

    Driver *            driver;         // Driver for the axis (useful for signaling events)
};

typedef struct mdrive_address mdrive_address_t;
struct mdrive_address {
    int                 speed;
    char                port[32];
    char                address;
};

enum checksum_mode {
    CK_OFF=0,
    CK_ON,
    CK_BUSY_NACK
};
typedef enum checksum_mode checksum_mode_t;

enum echo_mode {
    EM_ON=0,
    EM_PROMPT,
    EM_QUIET,
    EM_DELAY
};
typedef enum echo_mode echo_mode_t;

enum variable_persistence {
    TEMPORAL,
    PERSISTENT
};
typedef enum variable_persistence persistence_t;

// Special reads with mcQuery{Integer,String}
enum mdrive_read_variable {
    MDRIVE_SERIAL = 10000,
    MDRIVE_PART,
    MDRIVE_FIRMWARE,
    MDRIVE_MICROCODE,           // Microcode version (string)

    // Communication settings
    MDRIVE_BAUDRATE,
    MDRIVE_CHECKSUM,
    MDRIVE_ECHO,
    MDRIVE_ADDRESS,
    MDRIVE_NAME,                // Name a unit by S/N
    MDRIVE_RESET,               // Reboot
    MDRIVE_HARD_RESET,          // Factory defaults

    // Communication statistics
    MDRIVE_STATS_RX,
    MDRIVE_STATS_TX,

    // I/O Configuration
    MDRIVE_IO_TYPE,
    MDRIVE_IO_INVERT,
    MDRIVE_IO_DRIVE,
};

// Error codes
enum mdrive_error {
    MDRIVE_ENOTSUP = 20,
    MDRIVE_EINVAL = 21,
    MDRIVE_EOVERRUN = 63,
    MDRIVE_ETEMP = 71,
    MDRIVE_ESTALL = 86
};

// Tracing channel (source) names
#define CHANNEL "mdrive"
#define CHANNEL_RX CHANNEL ".rx"
#define CHANNEL_TX CHANNEL ".tx"

extern int MDRIVE_CHANNEL, MDRIVE_CHANNEL_TX, MDRIVE_CHANNEL_RX;

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
      _a < _b ? _a : _b; })

#endif
