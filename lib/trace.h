#ifndef TRACE_H
#define TRACE_H

#include "message.h"

#include <stdbool.h>

typedef void (*trace_callback_t)(int level, int channel, const char * buffer);

typedef struct trace_subscriber trace_subscriber_t;
struct trace_subscriber {
    int                 id;
    bool                active;
    trace_callback_t    callback;
    short               level;
    long long           channels;   // Mask of channels
    int                 pid;        // Client PID used for remote tracing
};

struct trace_channel {
    int                 id;
    bool                active;
    char                name[32];
    unsigned long       hash;
};

#define ALL_CHANNELS -1

void
mcTrace(int level, int channel, const char * buffer, int length);

extern void
mcTraceF(int level, int channel, const char * fmt, ...);

extern void
mcTraceBuffer(int level, int channel, const char * buffer,
        int length);

extern int
mcTraceSubscribe(int level, long long channel_mask, trace_callback_t callback);

extern int
mcTraceUnsubscribe(int id);

PROXYDEF(mcTraceSubscribeRemote, int, int level, long long mask);
PROXYDEF(mcTraceUnsubscribeRemote, int, int id);
PROXYDEF(mcTraceSubscribeAdd, int, int id, String * name);
PROXYDEF(mcTraceSubscribeRemove, int, int id, String * name);

#endif
