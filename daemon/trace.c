#include "trace.h"

#include "lib/trace.h"

int DAEMON_CHANNEL;

void
DaemonTraceInit(void) {
    DAEMON_CHANNEL = mcTraceChannelInit(DAEMON_CHANNEL_NAME);
}
