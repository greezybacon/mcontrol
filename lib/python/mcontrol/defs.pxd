cdef extern from "lib/message.h":
    ctypedef struct String:
        int         size
        char        buffer[256]

from constants cimport motion_increment
cdef extern from "time.h":
    ctypedef long int time_t

    struct timespec:
        int         tv_sec
        long int    tv_nsec

cdef extern from "lib/client.h" nogil:
    void mcClientTimeoutSet(timespec * new, timespec * old)
    int mcClientCallModeGet()
    void mcClientCallModeSet(int)
    void mcDriverLoad(char * path)

    int mcConnect(String * connection, int * m, int)
    int mcDisconnect(int)
    int mcMoveRelativeUnits(int motor, int measure, int units)
    int mcMoveAbsoluteUnits(int motor, int measure, int units)
    int mcSlewUnits(int motor, int rate, int units)
    int mcStop(int motor)
    int mcHalt(int motor)
    int mcEStop(int motor)

    int mcPokeString(int motor, int what, String * value)
    int mcPokeStringWithStringItem(int motor, int what, String * value,
        String * item)
    int mcQueryString(int motor, int what, String * value)
    int mcQueryInteger(int motor, int what, int * value)
    int mcQueryIntegerWithStringItem(int motor, int what, int * value,
        String * item)
    int mcQueryIntegerWithIntegerItem(int motor, int what, int * value,
        int item)
    int mcQueryFloat(int motor, int what, double * value)
    int mcPokeInteger(int motor, int what, int value)
    int mcPokeIntegerUnits(int motor, int what, int value, int units)
    int mcPokeIntegerWithStringItem(int motor, int what, int value,
        String * item)
    int mcPokeIntegerWithIntegerItem(int motor, int what, int value,
        int item)

    int mcUnitScaleSet(int motor, int units, long long urevs)
    int mcUnitScaleGet(int motor, int * units, long long * urevs)

    int mcSearch(int motor, String * driver, String * results)
    int mcFirmwareLoad(int motor, String * filename)
    int mcMicrocodeLoad(int motor, String * filename)

    int mcTraceSubscribeRemote(int, int level, long long mask)
    int mcTraceUnsubscribeRemote(int, int id)
    int mcTraceSubscribeAdd(int, int id, String * name)
    int mcTraceSubscribeRemove(int, int id, String * name)
    int mcTraceChannelEnum(int, String * channels)
    int mcTraceChannelLookupRemote(int, String * channel, int * id)

cdef extern from "lib/trace.h" nogil:
    ctypedef void (*trace_callback_t)(int id, int level, int channel, char * buffer)
    int mcTraceSubscribe(int level, long long channel_mask, trace_callback_t callback)
    int mcTraceUnsubscribe(int id)
    int mcTraceChannelLookup(char * name)

cdef struct event_trace_data:
    char    level
    char    channel
    char    buffer[254]

from libc.stdint cimport int64_t

cdef extern from "motor.h":
    ctypedef struct motion_t:
        unsigned    completed
        unsigned    stalled
        unsigned    cancelled
        unsigned    stopped
        unsigned    in_progress
        unsigned    pos_known
        unsigned    error
        long long   position

    ctypedef union event_data_t:
        long long number
        char      string[256]
        event_trace_data trace
        motion_t  motion
    ctypedef struct event_info_t:
        int     event
        int     motor
        event_data_t * data
        void *  user
    ctypedef void (*event_cb_t)(event_info_t *evt)
    ctypedef enum event_name:
        EV_MOTION
        EV_TRACE
    ctypedef enum error:
        ER_NO_DAEMON,
        ER_COMM_FAIL,
        ER_DAEMON_BUSY

cdef extern from "lib/events.h" nogil:
    int mcSubscribe(int, int, int *, event_cb_t)
    int mcUnsubscribe(int, int)
    int mcSubscribeWithData(int, int, int *, event_cb_t, void *)
    int mcEventWait(int, int)

cdef extern from "Python.h":
    void PyEval_InitThreads()

from libc.stdio cimport snprintf, printf

cdef inline String bufferFromString(string):
    cdef String buf
    string = string.encode('latin-1')
    # XXX: This results in a double copy
    buf.size = snprintf(buf.buffer, sizeof(buf.buffer), "%s", <char *>string)
    return buf
