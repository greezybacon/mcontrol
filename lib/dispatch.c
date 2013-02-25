#include "../motor.h"
#include "../drivers/driver.h"

#include "message.h"
#include "trace.h"

// Include the dispatch table generated in dispatch.h
#define DISPATCH_INCLUDE_TABLE
#include "dispatch.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int
LookupByMessageType(const void * c1, const void * c2) {
    return ((struct dispatch_table *)c1)->type
        - ((struct dispatch_table *)c2)->type;
}

/**
 * mcDispatchProxyMessage
 *
 * Used by the out-of-process daemon to find the proxy function used to
 * implement the message received from the stub call. The client.c code will
 * generate code to send a message to the daemon with the
 * function-to-be-called listed as a number (in the 'type' field of the
 * message). The number will line up with the TYPE_* numbers in the dispatch
 * table generated in dispatch.h.
 *
 * This routine will use a binary search algorithm (the table is
 * auto-generated in sort order) to find the function that corresponds to
 * the requested type number.
 *
 * Returns:
 * 0 upon success, -1 if type number is not listed in the dispatch table
 */
int
mcDispatchProxyMessage(request_message_t * message) {
    assert(message != NULL);

    // TYPE__LAST is the id of the last item + 1, TYPE__FIRST is the id of
    // the first item - 1. Therefore the number of items is LAST - FIRST - 1
    struct dispatch_table key = { .type = message->type },
        * e = bsearch(
            &key, table, TYPE__LAST - TYPE__FIRST - 1,
            sizeof(struct dispatch_table), LookupByMessageType);

    if (e) {
        mcTraceF(40, LIB_CHANNEL, "Dispatching call to %s", e->name);
        e->callable(message);
        return 0;
    }
    // else
    mcTraceF(10, LIB_CHANNEL, "Received unexpected request of type %d",
        message->type);
    return -1;
}
