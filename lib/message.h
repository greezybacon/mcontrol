#ifndef MESSAGE_H
#define MESSAGE_H

#include "../motor.h"
#include "motor.h"

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

// For the proxy-stub model each function will have four pieces:
//
// <function>
//      The function from the client's perspective. Will branch to the Stub
//      or directly to the Impl method depending on the client's configured
//      call-mode. For in-process clients, this will act as the stub method
//      and will deal with OUT argument marshalling.
// <function>Impl
//      The actual function implementation -- defined in the .c file. The
//      first argument to the Impl function will be a (struct call_context *)
//      which will contain meta information about how the function was
//      reached
// <function>Stub
//      The daemon-side of the proxy-stub model, which will trampoline to
//      the Impl method, unpacking the client's args, and marshalling the
//      OUT parameters and the return value back to the client
// <function>Proxy
//      The client-side of the proxy-stub model, which will use a POSIX
//      message queue to marshall the arguments to the remote server daemon
//
// All but the actual implementation are automatically generated into the
// client.c file by the client.c.py script
#define PROXYSTUB(type, func, ...) \
    extern type func(__VA_ARGS__); \
    extern void func##Stub(request_message_t *); \
    extern type func##Proxy(__VA_ARGS__); \
    extern type func##Impl(struct call_context *, __VA_ARGS__)

#define PROXYDEF(func, rettype, ...) \
    extern void func##Impl(request_message_t *)

#define CONTEXT (__context)

#define PROXYIMPL(func, ...) \
    func##Impl(struct call_context * __context, __VA_ARGS__)

// Used to call from one function impl to another
//#define CALL(func, ...) \
//    func#Impl(CONTEXT, __VA_ARGS__)

#define UNPACK_ARGS(func, local) \
    struct CONCATENATE(CONCATENATE(_,func),_args) \
        * local = (void *) message->payload, \
        * __args = local

#define RETURN(what) \
    do { \
        __args->returned = what; \
        if (CONTEXT->outofproc) \
            mcMessageReply(message, __args, sizeof *__args); \
        return; \
    } while(0)

#define MOTOR motor_t
#define OUT
#define IMPORTANT
#define SLOW

struct call_context {
    bool    inproc;
    bool    outofproc;
    int     caller_pid;
    Motor * motor;
};

// A couple of #defines to make calling driver class functions nicer looking
#define INVOKE(motor, func, ...) \
    motor->driver->class->func(motor->driver, __VA_ARGS__)
#define SUPPORTED(motor, func) \
    (motor->driver->class->func != NULL)

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
mcIsMessageAvailable(bool * available);

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

#endif
