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

PROXYSTUB(int, mcTraceSubscribeRemote, int level, unsigned 
    long long mask);
PROXYSTUB(int, mcTraceUnsubscribeRemote, int id);
PROXYSTUB(int, mcTraceSubscribeAdd, int id, String * name);
PROXYSTUB(int, mcTraceSubscribeRemove, int id, String * name);
PROXYSTUB(int, mcTraceChannelEnum, OUT String * channels);
PROXYSTUB(int, mcTraceChannelLookupRemote, String * buffer, OUT int * id);
PROXYSTUB(int, mcTraceChannelGetNameRemote, int id, OUT String * buffer);

#endif
