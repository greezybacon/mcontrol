#ifndef TRACE_H
#define TRACE_H

#include "message.h"

#include <stdbool.h>

typedef void (*trace_callback_t)(int id, int level, int channel, const char * buffer);

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

extern int LIB_CHANNEL;
#define LIB_CHANNEL_NAME "libmcontrol"

extern int
mcTraceChannelInit(const char * name);

void
mcTrace(int level, int channel, const char * buffer, int length);

extern void
mcTraceF(int level, int channel, const char * fmt, ...);

extern void
mcTraceBuffer(int level, int channel, const char * buffer,
        int length);

extern int
mcTraceSubscribe(int level, unsigned long long channel_mask,
    trace_callback_t callback);

extern int
mcTraceUnsubscribe(int id);

extern char *
mcTraceChannelGetName(int id);

PROXYDEF(mcTraceSubscribeRemote, int, int level, unsigned 
    long long mask);
PROXYDEF(mcTraceUnsubscribeRemote, int, int id);
PROXYDEF(mcTraceSubscribeAdd, int, int id, String * name);
PROXYDEF(mcTraceSubscribeRemove, int, int id, String * name);
PROXYDEF(mcTraceChannelEnum, int, OUT String * channels);
PROXYDEF(mcTraceChannelLookupRemote, int, String * buffer, OUT int * id);
PROXYDEF(mcTraceChannelGetNameRemote, int, int id, OUT String * buffer);

#endif
