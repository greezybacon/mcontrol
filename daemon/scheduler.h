#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../lib/message.h"

typedef int (*Scheduler)(request_message_t * message);

extern int
GitRDone(request_message_t * message);

extern int
SchedulerConfigure(void);

#endif
