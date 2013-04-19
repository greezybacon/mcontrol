#include "drivers/driver.h"

#include "lib/client.h"
#include "lib/driver.h"
#include "lib/message.h"
#include "lib/trace.h"

#include "controller.h"
#include "trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

void cleanup(int signal) {
    exit(0);
}

void
trace_output(int id, int level, int channel, const char * buffer) {
    fprintf(stderr, "%d: %s\n", channel, buffer);
}

int main(int argc, char * argv[]) {
    DaemonTraceInit();
    mcTraceSubscribe(50, ALL_CHANNELS, trace_output);

    // Ensure that calls made from the daemon stay in process
    mcClientCallModeSet(MC_CALL_IN_PROCESS);

    mcDriverLoad("mdrive.so");
    mcDriverLoad(DRIVER_DIR "/mdrive.so");

    signal(SIGINT, cleanup);
    signal(SIGHUP, cleanup);

    mcMessageBoxOpenDaemon();

    receive_messages(false);
    
    mq_unlink(DAEMON_QUEUE_NAME);

    /*
    // Invoke a driver search
    DriverClass * mdrive = driver_class_lookup("mdrive");
    char strings[2048];
    driver_class_search(mdrive, strings, sizeof strings);

    printf("Found %s\n", strings);
    */
    
    exit(0);
}
