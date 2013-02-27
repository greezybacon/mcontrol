#include "../motor.h"

#include "message.h"

#include "events.h"
#include "trace.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stddef.h>
#include <signal.h>

static mqd_t _inbox = 0;        // Client's queue for return traffic,
                                //      server's queue for incoming traffic
static mqd_t _outbox = 0;       // Server's queue from client's perpective
static unsigned long message_id = 1;

static bool _async_registered = false;

static struct mq_attr _inbox_attr = {
    .mq_msgsize = sizeof(request_message_t),
    .mq_maxmsg = 8         // Backlog
};

static int
construct_request_raw(request_message_t *, int, void *, int);

static int
construct_response(request_message_t *, response_message_t *, void *, int);

static int
construct_request(motor_t, request_message_t *, int, void *, int);

void __attribute__((constructor))
mcInitialize(void) {
    mcMessageBoxOpen();
}

static void __attribute__((destructor))
mcGoodBye(void) {
    char buffer[64];

    // Create queue for the client and events
    snprintf(buffer, sizeof buffer, CLIENT_QUEUE_FMT, getpid(), "wait");
    mq_unlink(buffer);
}

static void
mcRudeGoodBye(int signal) {
    // Run all the destructors
    exit(0);
}

int
mcMessageBoxOpen(void) {
    char buffer[64];

    snprintf(buffer, sizeof buffer, CLIENT_QUEUE_FMT, getpid(), "wait");
    _inbox = mq_open(buffer, O_CREAT | O_EXCL, 0600, &_inbox_attr);
    if (_inbox == -1)
        mcTraceF(10, LIB_CHANNEL, "Unable to open event queue: %s",
            strerror(errno));
    else {
        // Make sure mcGoodBye is called at program exit
        struct sigaction action = { .sa_handler = mcRudeGoodBye };
        sigaction(SIGTERM, &action, NULL);
        sigaction(SIGQUIT, &action, NULL);
        sigaction(SIGHUP, &action, NULL);
    }
    return (_inbox > 0) ? 0 : -1;
}

int
mcMessageBoxOpenDaemon(void) {
    _inbox = mq_open(DAEMON_QUEUE_NAME, O_CREAT, 0600, &_inbox_attr);
    if (_inbox == 0)
        mcTraceF(10, LIB_CHANNEL, "Unable to open inbox: %s",
            strerror(errno));
    else
        mcTraceF(30, LIB_CHANNEL, "Opened inbox %s", DAEMON_QUEUE_NAME);

    // XXX: Make sure inbox was successfully opened
    return (_inbox > 0) ? 0 : -1;
}

void
tsAdd(const struct timespec * a, const struct timespec * b,
        struct timespec * total) {
    total->tv_sec = a->tv_sec + b->tv_sec;
    total->tv_nsec = a->tv_nsec + b->tv_nsec;
    if (total->tv_nsec > (int)1e9) {
        total->tv_nsec -= (int)1e9;
        total->tv_sec++;
    }
}

long long
nsecDiff(struct timespec * a, struct timespec * b) {
    return (a->tv_sec - b->tv_sec) * (int)1e9
        + (a->tv_nsec - b->tv_nsec);
}

int
mcMessageSend(motor_t motor, int type,
        void * payload, int payload_size,
        int priority,
        const struct timespec * timeout) {
    int status;

    if (_outbox == 0) {
        _outbox = mq_open(DAEMON_QUEUE_NAME, O_WRONLY);
        if (_outbox <= 0)
            return -errno;
    }

    request_message_t message;
    status = construct_request(motor, &message,
        type, payload, payload_size);

    if (timeout == NULL) {
        status = mq_send(_outbox, (void *) &message,
            message.size, priority);
    // Send message with a timeout
    } else {
        struct timespec now, then;
        if (timeout->tv_sec < 100) {
            // Time is relative and should be absolute
            clock_gettime(CLOCK_REALTIME, &now);
            tsAdd(&now, timeout, &then);
        } else {
            then = *timeout;
        }
        status = mq_timedsend(_outbox, (void *) &message,
            message.size, PRIORITY_CMD, &then);
    }

    if (status == -1) {
        switch (errno) {
            case EMSGSIZE:
                // Message was too big. Operating system needs to be
                // configured to handle larger messages
            case EINTR:
                // Send was interrupted
                break;
        }
        return -errno;
    }
    return message.id;
}

/**
 * mcEventSend
 *
 * Used server-side (daemon) to deliver an event message to a client. The
 * event message is a special (struct response_message) format sent with
 * priority PRIORITY_EVENT. The payload is a (struct event_message) which
 * contains the specifics of the event signaled from a motor driver.
 *
 * Returns:
 * (int) - size of the receive message, or -1 to signal error
 */
int
mcEventSend(int pid, struct event_message * evt) {
    int status;
    struct response_message m = {
        .id = -1,
        .payload_size = sizeof *evt,
        .size = offsetof(struct request_message, payload) + sizeof *evt,
    };
    memcpy(m.payload, evt, sizeof *evt);

    // Find the client's wait queue
    char boxname[64];
    snprintf(boxname, sizeof boxname, CLIENT_QUEUE_FMT, pid, "wait");

    // The client must be able to receive the event immediately. If the
    // client is blocked and the message queue for the client is full, then
    // the event will not be delivered
    mqd_t client_inbox = mq_open(boxname, O_WRONLY | O_NONBLOCK);
    if (client_inbox == -1)
        return -errno;
    
    status = mq_send(client_inbox, (void *)&m,
        m.size, PRIORITY_EVENT);

    mq_close(client_inbox);

    if (status == -1)
        return -errno;
    
    return 0;
}

/**
 * mcAsyncReceiveCancel
 *
 * Cancels the registration to receive a message asynchronously. This is
 * intended to be done internally when performing a blocking receive after
 * an async registration was completed.
 *
 * Returns
 * (bool) - TRUE if the registration was canceled and false otherwise
 */
static bool
mcAsyncReceiveCancel(void) {
    if (_async_registered && (0 == mq_notify(_inbox, NULL))) {
        _async_registered = false;
        return true;
    }
    return false;
}

int
mcMessageSendWait(motor_t motor, int type,
        void * payload, int payload_size,
        int priority,
        response_message_t * response,
        const struct timespec * timeout) {

    bool redo_async = mcAsyncReceiveCancel();
    int msg_id;
    struct timespec now, then, * pTimeout = NULL;

    if (timeout != NULL) {
        // Fetch current time and calculate time of timeout
        clock_gettime(CLOCK_REALTIME, &now);
        tsAdd(&now, timeout, &then);
        pTimeout = &then;
    }
    msg_id = mcMessageSend(motor, type, payload, payload_size, priority,
        pTimeout);

    if (msg_id < 0) {
        mcTraceF(10, LIB_CHANNEL, "Unable to queue message: %s",
            strerror(-msg_id));
        return msg_id;
    }

    int size = mcResponseReceive(response, pTimeout);

    if (redo_async)
        mcAsyncReceive();

    return size;
}

void
mcMessageReply(request_message_t * message, void * payload, int payload_size) {
    response_message_t response;
    
    int status = construct_response(message, &response,
        payload, payload_size);

    // Find the client's wait queue
    char boxname[64];
    snprintf(boxname, sizeof boxname, CLIENT_QUEUE_FMT, message->pid, "wait");

    mqd_t client_inbox = mq_open(boxname, O_WRONLY);
    if (client_inbox == -1)
        return;
    
    status = mq_send(client_inbox, (void *)&response,
        response.size, PRIORITY_CMD);

    mq_close(client_inbox);

    if (status == -1) {
        switch (errno) {
            case EMSGSIZE:
                // Message was too big. Mailbox needs to be configured to
                // receive larger messages
            break;
        }
    }
}

static void
_on_mg_notify(int signal, siginfo_t * info, void * context) {
    if (info->si_code != SI_MESGQ)
        // Whoops. Misdirected signal
        return;

    response_message_t msg;
    mcResponseReceive2(&msg, false, NULL);

    // If msg was not an event, then it was unsolicited and will be
    // discarded here.
    if (msg.id != -1)
        mcTraceF(20, LIB_CHANNEL, "Discarding unsolicted response: %d",
            msg.id);
}

/**
 * mcAsyncReceive
 *
 * Register to receive a message asynchronously. By default, the
 * _on_mg_notify function will receive notification of message availability.
 * This is only useful for receiving events, because it is intended for
 * client side use and responses are generally waited upon rather than
 * asynchronously recieved.
 */
void
mcAsyncReceive(void) {
    static struct sigaction config = {
        .sa_sigaction = _on_mg_notify,
        .sa_flags = SA_SIGINFO
    };
    static struct sigevent onevent = {
        .sigev_notify = SIGEV_SIGNAL,
        .sigev_signo = SIGUSR2
    };
    if (_async_registered)
        return;

    sigemptyset(&config.sa_mask);

    if (0 != sigaction(SIGUSR2, &config, NULL))
        mcTraceF(10, LIB_CHANNEL, "Could not register signal handler: %s",
            strerror(errno));
    if (0 == mq_notify(_inbox, &onevent))
        _async_registered = true;
    else
        mcTraceF(10, LIB_CHANNEL, "Could not receive async: %s",
            strerror(errno));
}

int
mcIsMessageAvailable(bool * available) {
    struct mq_attr attrs;
    int status = mq_getattr(_inbox, &attrs);
    if (status)
        return status;

    *available = attrs.mq_curmsgs > 0;
    return 0;
}

int
mcResponseReceive2(response_message_t * response, bool message,
        const struct timespec * timeout) {
    unsigned int priority, length;

    // Blocking receive -- cancel async notification
    bool redo_async = mcAsyncReceiveCancel();

    struct timespec now, abstimeout, *then = &abstimeout;

    // Properly handle the originally referenced timeout: if an event is
    // received, the timeout will be restarted. The target absolute time
    // should only be calculated once.
    if (timeout != NULL) {
        if (timeout->tv_sec < 100) {
            // Time is relative and should be absolute
            clock_gettime(CLOCK_REALTIME, &now);
            tsAdd(&now, timeout, then);
        } else {
            *then = *timeout;
        }
    }

    do {
        if (timeout)
            length = mq_timedreceive(_inbox, (void *) response,
                sizeof *response, &priority, then);
        else
            length = mq_receive(_inbox, (void *) response,
                sizeof *response, &priority);
            
        if (length == -1)
            return -errno;

        // Handle events directly from here (client side)
        if (priority == PRIORITY_EVENT)
            mcDispatchSignaledEventMessage(response);
        else
            // If event was received when a message was expected, another
            // call to mq_receive should be made until a message is received
            // instead of an event
            break;
    } while (message);

    if (redo_async)
        mcAsyncReceive();

    return length;
}

int
mcResponseReceive(response_message_t * response,
        const struct timespec * timeout) {
    return mcResponseReceive2(response, true, timeout);
}

int
mcMessageReceive(request_message_t * message) {
    unsigned int priority, length;

    length = mq_receive(_inbox, (void *) message, sizeof *message,
        &priority);

    if (length == -1)
        return -errno;

    return length;
}

void
mcInboxExpunge(void) {
    // Configure inbox for non blocking
    struct mq_attr attributes;
    mq_getattr(_inbox, &attributes);

    attributes.mq_flags |= O_NONBLOCK;
    mq_setattr(_inbox, &attributes, NULL);

    request_message_t message;
    while (mcMessageReceive(&message) > 0);

    // Unset non blocking
    attributes.mq_flags &= ~O_NONBLOCK;
    mq_setattr(_inbox, &attributes, NULL);
}

static int
construct_request_raw(request_message_t * message, int type,
        void * payload, int payload_size) {

    * message = (struct request_message) {
        .pid = getpid(),
        .type = type,
        .id = message_id++,
        .payload_size = payload_size,
        .size = offsetof(struct request_message, payload) + payload_size
    };

    if (payload_size > sizeof message->payload)
        return EMSGSIZE;
        
    else if (payload && payload_size)
        memcpy(message->payload, payload, payload_size);

    return 0;
}

static int
construct_request(motor_t motor, request_message_t * message,
        int type, void * payload, int payload_size) {

    int status = construct_request_raw(message, type, payload, payload_size);

    if (status == 0)
        message->motor_id = motor;

    return status;
}

static int
construct_response(request_message_t * message, response_message_t * response,
        void * payload, int payload_size) {

    * response = (struct response_message) {
        .id = message->id,
        .payload_size = payload_size,
        .size = offsetof(struct response_message, payload) + payload_size
    };

    if (payload_size > sizeof response->payload)
        return EMSGSIZE;
        
    if (payload && payload_size)
        memcpy(response->payload, payload, payload_size);

    return 0;
}
