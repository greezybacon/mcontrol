#include "../drivers/driver.h"
#include "../lib/message.h"
#include "../lib/dispatch.h"

#include "request.h"
#include "controller.h"

#include <stdbool.h>
#include <mqueue.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * async_receive
 *
 * Simple trampoline function that will be invoked when new messages arrive
 * in the inbox message queue. This function should just call
 * receive_messages(), which will receive and handle all the messages on the
 * queue and re-register to receive async notifications again.
 */
void
async_receive(union sigval val) {
    receive_messages(true);
}

void
async_register(mqd_t inbox) {

    static struct mq_attr attrs = { .mq_msgsize=0 };
    if (attrs.mq_msgsize == 0 ) {
        mq_getattr(inbox, &attrs);
    }

    // TODO: Place the queue in non-blocking mode
    attrs.mq_flags = O_NONBLOCK;
    mq_setattr(inbox, &attrs, NULL);

    struct sigevent registration = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value = inbox,
        .sigev_notify_function = async_receive
    };

    mq_notify(inbox, &registration);
}

/*
int
handle_message(request_message_t * message) {
    printf("Received a message of type: %d\n", message->type);
    switch (message->type) {
        case REQ_MOVE:
            return handle_move_request(message);
        case REQ_CONNECT:
            return handle_connect_request(message);
    }
}
*/

void
receive_messages(bool async) {
    int length;
    unsigned priority;

    request_message_t message;

    // Drop stale messages
    mcInboxExpunge();

    // TODO: (Re)register for asynchronous notification
    printf("Open for business, waiting for messages\n");

    // Drain the inbox
    while (true) {
        // Retrieve the message and priority
        length = mcMessageReceive(&message);

        if (length == -1) {
            switch (errno) {
                case EAGAIN:
                    // No more messages on the queue
                    break;
                default:
                    printf("Received error from mcMessageReceive\n");
            }
        }

        // TODO: Handle errors for the message receive

        if (message.type > TYPE__FIRST)
            mcDispatchProxyMessage(&message);
    }
}
