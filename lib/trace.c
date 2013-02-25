#include "../motor.h"

#include "trace.h"

#include "message.h"
#include "events.h"
#include "client.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Max number of characters written per trace call (for formatted messages
// using mcTraceF). Remote clients will be limited by the buffer size
// declared in the event_data.event structure.
static const int MAX_TRACE_SIZE = 512;

static const int MAX_TRACE_CHANNELS = sizeof(unsigned long long) * 8;
static struct trace_channel * trace_channels = NULL;
static int trace_channel_count = 0;
static int trace_channel_serial = 0;

static const int MAX_SUBSCRIBERS = sizeof(unsigned long long) * 8;
static struct trace_subscriber * subscribers = NULL;
static int subscriber_count = 0;
static int subscriber_serial = 0;

// Create and use a lock for two reasons:
// 1. Used to lock subscription of new subscribers between a call to
//    mcTraceSubscribersLookup() and mcTraceWrite(). Also to be used for
//    unsubscription, which is more likely to become a problem.
// 2. Used to lock mcTraceWrite so calls from multiple threads are
//    serialized to help ensure that the subscriber callback functions need
//    not be thread safe.
static pthread_mutex_t trace_lock = PTHREAD_MUTEX_INITIALIZER;

static inline void
mcTraceLock(void) {
    pthread_mutex_lock(&trace_lock);
}

static inline void
mcTraceUnlock(void) {
    pthread_mutex_unlock(&trace_lock);
}

/**
 * mcTraceSubscribersLookup
 *
 * Returns bitmask of subscribers who request to receive messages of the
 * given level and channel. This bitmask should be passed into mcTraceWrite
 * sometime later.
 */
static unsigned long long
mcTraceSubscribersLookup(int level, int channel) {
    if (subscribers == NULL || subscriber_count == 0)
        return 0;

    unsigned long long
        mask = 0, channel_mask = 1ULL << (channel - 1),
        SET = 1ULL << (((sizeof mask) * 8) - 1);
    int count = 0;
    struct trace_subscriber * s = subscribers;

    // Walk to high-water mark using the id field, which isn't unset for
    // unsubcriptions
    while (s->id) {
        count++;
        mask >>= 1;
        if (s->active && s->level >= level && s->channels & channel_mask)
            mask |= SET;
        s++;
    }
    mask >>= ((sizeof mask) * 8) - count;

    return mask;
}

int
mcTraceRemoteWrite(int pid, int level, int channel, const char * buffer) {

    struct event_message evt = {
        .motor = -1,    // XXX: Use a constant like ANY_MOTOR
        .event = EV_TRACE,
        .id = level
    };
    // TODO: Decide where level argument should be placed in the event
    //       message payload...
    evt.data.trace.level = level;
    evt.data.trace.channel = channel;
    snprintf(evt.data.trace.buffer, sizeof evt.data.trace.buffer,
        "%s", buffer);

    return mcEventSend(pid, &evt);
}

static int
mcTraceWrite(unsigned long long mask, int level, int channel,
        const char * buffer, int length) {

    // Actually write the trace buffer to the subscribers
    struct trace_subscriber * s = subscribers;
    int status;
    while (mask) {
        if (mask & 1) {
            if (s->callback)
                s->callback(s->id, level, channel, buffer);
            else if (s->pid) {
                status = mcTraceRemoteWrite(s->pid, level, channel, buffer);
                if (status == -ENOENT)
                    // Client went away -- and didn't say bye!
                    s->active = false;
            }
        }
        s++;
        mask >>= 1;
    }
    return 0;
}

/**
 * mcTrace
 *
 * Backend trace call, used to emit traces to subscribers
 */
void
mcTrace(int level, int channel, const char * buffer, int length) {
    mcTraceLock();

    long long mask = mcTraceSubscribersLookup(level, channel);
    if (mask)
        mcTraceWrite(mask, level, channel, buffer, length);

    mcTraceUnlock();
}

void
mcTraceF(int level, int channel, const char * fmt, ...) {
    va_list args;

    mcTraceLock();

    long long mask = mcTraceSubscribersLookup(level, channel);
    if (mask) {
        char * buffer;
        int length;

        buffer = malloc(MAX_TRACE_SIZE);

        va_start(args, fmt);
        length = vsnprintf(buffer, MAX_TRACE_SIZE, fmt, args);
        va_end(args);

        mcTraceWrite(mask, level, channel, buffer, length);

        free(buffer);
    }
    mcTraceUnlock();
}

/**
 * mcTraceBuffer
 *
 * Emits a trace with the received buffer pretty printed similar to a
 * hexdump output -- maximum size is MAX_TRACE_SIZE / 64 (96 bytes if
 * MAX_TRACE_SIZE is 512):
 *
 * 61 4d 52 20 32 35 36 30 30 30 30 83 0a           aMR 2560000.
 */
void
mcTraceBuffer(int level, int channel, const char * buffer,
        int length) {

    if (length < 1)
        return;

    mcTraceLock();

    unsigned long long mask = mcTraceSubscribersLookup(level, channel);
    if (!mask) {
        mcTraceUnlock();
        return;
    }

    char buf[MAX_TRACE_SIZE], raw[49], ascii[17];
    char * pbuf = buf, * praw = raw, * pascii = ascii;
    int count;

    while (length--) {
        // Write the raw and ascii versions of the character to
        // praw and pascii pointers
        praw += sprintf(praw, "%02x ", (unsigned)(unsigned char)*buffer);
        if (*buffer > 31 && *buffer < 128)
            *pascii++ = *buffer;
        else
            *pascii++ = '.';

        // Advance the buffer pointer and flush after 16 chars or end of
        // buffer reached
        buffer++;
        if (length == 0 || pascii - ascii >= 16) {
            // Null-terminate and flush to output buffer
            *praw = 0;
            *pascii = 0;
            count = snprintf(pbuf, sizeof buf + pbuf - buf,
                "%s%-48s %-16s",
                (pbuf > buf) ? "\n" : "", // Second line+ prefix with \n
                raw, ascii);

            // Detect end of output buffer
            if (count < 49)
                break;
            else
                pbuf += count;

            // Reset points for raw and ascii buffers
            praw = raw;
            pascii = ascii;
        }
    }

    mcTraceWrite(mask, level, channel, buf, pbuf - buf);
    mcTraceUnlock();
}

static void __attribute__((constructor))
mcTraceSystemInit(void) {
    // Allocate the list here on the heap so that everything that links
    // against this library will not have to allocate memory for trace
    // subscription
    // Keep an extra as end-of-list sentinel
    subscribers = calloc(MAX_SUBSCRIBERS+1, sizeof *subscribers);
}

int
mcTraceSubscribe(int level, unsigned long long channel_mask,
        trace_callback_t callback) {
    
    if (subscriber_count == MAX_SUBSCRIBERS)
        return -ER_TOO_MANY;
    
    // Find first inactive subscriber entry
    struct trace_subscriber * s = subscribers;
    while (s->active) s++;

    *s = (struct trace_subscriber) {
        .id =       ++subscriber_serial,
        .active =   true,
        .callback = callback,
        .level =    level,
        .channels = channel_mask
    };

    subscriber_count++;

    return s->id;
}

int
mcTraceUnsubscribe(int id) {
    // Find first inactive subscriber entry
    struct trace_subscriber * s = subscribers;
    while (s->id && s->id != id) s++;

    if (s->id == 0)
        // Subscription id not found
        return EINVAL;

    subscriber_count--;
    s->active = false;

    return 0;
}

// Thanks, http://stackoverflow.com/a/7700425
static unsigned long
hashstring(const char * key) {
    if (key == NULL)
        return EINVAL;

    unsigned long hash = 5381;
    int c;
    while ((c = *key++))
        // hash = hash * 33 + c
        hash = ((hash << 5) + hash) + c;

    return hash;
}

int
mcTraceChannelInit(const char * name) {
    if (trace_channel_count == MAX_TRACE_CHANNELS)
        return -ER_TOO_MANY;

    if (trace_channels == NULL)
        trace_channels = calloc(MAX_TRACE_CHANNELS+1, sizeof *trace_channels);

    // Find first inactive channel descriptor
    struct trace_channel * c = trace_channels;
    while (c->active) c++;

    snprintf(c->name, sizeof c->name, "%s", name);
    c->id = ++trace_channel_serial;
    c->hash = hashstring(c->name);
    c->active = true;

    return c->id;
}

int
mcTraceChannelLookup(const char * name) {
    unsigned long hash = hashstring(name);

    struct trace_channel * c = trace_channels;
    while (c && c->id) {
        if (c->active && c->hash == hash)
            if (strncmp(c->name, name, sizeof c->name) == 0)
                return c->id;
        c++;
    }

    return -ENOENT;
}

// Remote clients
/**
 * mcTraceSubscribeRemote
 *
 * Used by clients to attach to trace events. Trace messages will be
 * delivered via the default event mechanisms. Therefore, the client will
 * still need to subscribe to the EV_TRACE event and declare a callback
 * function or wait for the events to arrive in a threaded loop.
 */
int
PROXYIMPL(mcTraceSubscribeRemote, int level, unsigned long long mask) {

    int id = mcTraceSubscribe(level, mask, NULL);

    if (id == -ER_TOO_MANY)
        return -ER_TOO_MANY;

    // Find the id in the subscriber array
    struct trace_subscriber * s = subscribers;
    while (s->id && s->id != id) s++;
    
    s->pid = CONTEXT->caller_pid;
    s->channels = 0;

    return id;
}

int
PROXYIMPL(mcTraceUnsubscribeRemote, int id) {
    return mcTraceUnsubscribe(id);
}

int
PROXYIMPL(mcTraceSubscribeAdd, int id, String * name) {

    // Find the id in the subscriber array
    struct trace_subscriber * s = subscribers;
    while (s->id && s->id != id) s++;

    int channel = mcTraceChannelLookup(name->buffer);
    if (channel == -ENOENT)
        return -ENOENT;

    s->channels |= 1 << (channel - 1);
    return 0;
}

int
PROXYIMPL(mcTraceSubscribeRemove, int id, String * name) {

    // Find the id in the subscriber array
    struct trace_subscriber * s = subscribers;
    while (s->id && s->id != id) s++;

    int channel = mcTraceChannelLookup(name->buffer);
    if (channel == -ENOENT)
        return -ENOENT;

    s->channels &= ~(1 << (channel - 1));
    return 0;
}

int
PROXYIMPL(mcTraceChannelEnum, String * channels) {

    char * pos = channels->buffer, * start = pos;
    int count = 0;

    struct trace_channel * c = trace_channels;
    while (c->id) {
        pos += snprintf(pos, sizeof channels->buffer + start - pos, "%s",
            c->name) + 1;
        c++;
        count++;
    }
    channels->size = pos - start;

    return count;
}
