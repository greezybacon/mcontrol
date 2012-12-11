cdef extern from "lib/message.h":
    ctypedef struct String:
        int         size
        char        buffer[256]

cdef extern from "stdbool.h":
    ctypedef struct bool

from constants cimport motion_increment

cdef extern from "lib/client.h" nogil:
    int mcConnect(String * connection, int * m)
    int mcMoveRelativeUnits(int motor, int measure, int units)
    int mcMoveAbsoluteUnits(int motor, int measure, int units)
    int mcPokeString(int motor, int what, String * value)
    int mcPokeStringItem(int motor, int what, String * value, String * item)
    int mcQueryString(int motor, int what, String * value)
    int mcQueryInteger(int motor, int what, int * value)
    int mcPokeInteger(int motor, int what, int value)
    int mcUnitScaleSet(int motor, int units, long long urevs)
    int mcUnitScaleGet(int motor, int * units, long long * urevs)

    int mcSearch(int motor, String * driver, String * results)
    int mcFirmwareLoad(int motor, String * filename)
    int mcMicrocodeLoad(int motor, String * filename)

    int mcTraceSubscribeRemote(int, int level, long long mask)
    int mcTraceUnsubscribeRemote(int, int id)
    int mcTraceSubscribeAdd(int, int id, String * name)
    int mcTraceSubscribeRemove(int, int id, String * name)

cdef struct event_trace_data:
    char    level
    char    channel
    char    buffer[254]

from libc.stdint cimport int64_t

cdef extern from "motor.h":
    ctypedef union event_user_data:
        int64_t     number
        char        string[256]
        event_trace_data trace
    ctypedef struct event_data_t:
        int     event
        int     motor
        event_user_data * event_data
        void *      user_data
    ctypedef void (*event_cb_t)(event_data_t *evt)
    ctypedef enum event_name:
        EV_TRACE
    ctypedef enum error:
        ER_NO_DAEMON,
        ER_COMM_FAIL,
        ER_DAEMON_BUSY

cdef extern from "lib/events.h" nogil:
    int mcSubscribe(int, event_name, event_cb_t)
    int mcUnsubscribe(int, event_name, event_cb_t)
    int mcSubscribeWithData(int, event_name, event_cb_t, void *)

from libc.stdio cimport snprintf, printf

cdef inline String bufferFromString(string):
    cdef String buf
    string = string.encode('latin-1')
    # XXX: This results in a double copy
    buf.size = snprintf(buf.buffer, sizeof(buf.buffer), "%s", <char *>string)
    return buf
