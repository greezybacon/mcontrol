#include "../motor.h"
#include "../drivers/driver.h"

#include "message.h"

// Include the dispatch table generated in dispatch.h
#define DISPATCH_INCLUDE_TABLE
#include "dispatch.h"

#include <stdio.h>

int
mcDispatchProxyMessage(request_message_t * message) {
    for (struct dispatch_table * e = table; e->type; e++) {
        if (e->type == message->type) {
            e->callable(message);
            return 0;
        }
    }
    printf("Received unexpected request of type %d\n", message->type);
    return -1;
}
