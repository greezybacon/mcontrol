#ifndef MESSAGE_H
#define MESSAGE_H

#include "../motor.h"

#include <unistd.h>
#include <stdbool.h>

enum request_type {
    REQ_CONNECT=1,
    REQ_COMMAND,
    REQ_MOVE,
    REQ_QUERY,
    REQ_SET_PROFILE
};
typedef enum request_type request_t;

#define PRIORITY_CMD 0
#define PRIORITY_EVENT 1
#define PRIORITY_HIGH 2

typedef struct string String;
struct string {
    int size;
    char buffer[256];
};

typedef struct request_message request_message_t;
struct request_message {
    int32_t pid;            // Connected client
    int32_t id;             // Unique identifier of the message
    int32_t motor_id;       // Motor to receive the request

    bool    waiting;        // TRUE if client is waiting for a response

    int32_t type;           // What is requested (MOVE, etc)

    int16_t size;           // Total size of the message

    int16_t payload_size;   // Length of payload
    char    payload[1024];  // Payload follows
};

typedef struct response_message response_message_t;
struct response_message {
    int32_t id;             // Message id of response
    int32_t status;         // Indication of success, ...

    int16_t size;           // Total size of the message

    int16_t payload_size;   // Size of payload
    
    // XXX: The response_message and request_message need to be the same
    //      size! Use this math to keep them the same size, but make sure
    //      the amount subtracted is always equal to the total size of the
    //      above members.
    char    payload[sizeof(request_message_t) - 12];  // Payload to follow
};

struct event_message {
    motor_t         motor;          // Device signaling the event
    int32_t         id;             // Subscription owner of the event
    event_t         event;          // Type of event
    union event_data data;
};

// Thanks to http://stackoverflow.com/a/1872506
#define CONCATENATE(arg1, arg2)   CONCATENATE1(arg1, arg2)
#define CONCATENATE1(arg1, arg2)  CONCATENATE2(arg1, arg2)
#define CONCATENATE2(arg1, arg2)  arg1##arg2

#define PROXYDEF(func, rettype, ...) \
    extern void func##Impl(request_message_t * message) // ...

#define PROXYIMPL(func, ...) \
    void func##Impl(request_message_t * message)

#define UNPACK_ARGS(func, local) \
    struct CONCATENATE(CONCATENATE(_,func),_args) \
        * local = (void *) message->payload, \
        * __args = local; \
    motor_t motor = message->motor_id

#define RETURN(what) \
    do { \
        args->returned = what; \
        if (__args->outofproc) \
            mcMessageReply(message, args, sizeof *args); \
        return; \
    } while(0)

#define MOTOR
#define OUT
#define IMPORTANT

extern int
mcMessageBoxOpen(void);

extern int
mcMessageSend(motor_t motor, int type,
    void * payload, int payload_size, int priority,
    const struct timespec * timeout);

extern int
mcMessageSendWait(motor_t motor, int type,
    void * payload, int payload_size, int priority,
    response_message_t * response, const struct timespec * timeout);

extern int
mcMessageReceive(request_message_t * message);

extern void
mcInboxExpunge(void);

extern void
mcAsyncReceive(void);

extern int
mcResponseReceive(response_message_t * response,
    const struct timespec * timeout);

extern int
mcResponseReceive2(response_message_t * response, bool wait_message,
    const struct timespec * timeout);

extern int
mcEventSend(int pid, struct event_message * evt);

extern void
mcMessageReply(request_message_t * message, void * payload, int payload_size);

static int
construct_request_raw(request_message_t * message, int type, void * payload,
    int payload_size);

static int
construct_request(motor_t motor, request_message_t * message, int type,
    void * payload, int payload_size);

static int
construct_response(request_message_t * message, response_message_t * response,
    void * payload, int payload_size);

#endif
