#include "../motor.h"
#include "../drivers/driver.h"

#include "message.h"

// Include the dispatch table generated in dispatch.h
#define DISPATCH_INCLUDE_TABLE
#include "dispatch.h"

#include <assert.h>
#include <stdio.h>

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
    // TYPE__LAST is the id of the last item + 1, TYPE__FIRST is the id of
    // the first item - 1. Therefore the number of items is LAST - FIRST - 1
    int imin = 0, imax = TYPE__LAST - TYPE__FIRST - 1, imid;
    struct dispatch_table * e = table;

    assert(message != NULL);

    while (imax >= imin) {
        // Estimate midpoint of the list
        imid = ((imax - imin) >> 1) + imin;
        assert(imid < imax);

        if ((e+imid)->type < message->type)
            imin = imid + 1;
        else
            imax = imid;
    }
    if (imax == imin && e->type == message->type) {
        e->callable(message);
        return 0;
    }
    // else
    printf("Received unexpected request of type %d\n", message->type);
    return -1;
}
