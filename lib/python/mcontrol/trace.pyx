from mcontrol.defs cimport *

cdef int ANY_MOTOR = -1

import logging

cdef void pyTraceCallback(event_info_t * info) with gil:
    if info.event != EV_TRACE:
        # Spurious event
        return

    cdef bytes text = info.data.trace.buffer

    assert info.user != NULL
    (<object>info.user).emit(
        info.data.trace.level,
        info.data.trace.channel, text.decode('latin-1'))

cdef class Trace:
    cdef int id
    cdef int event_id

    def __init__(self, level=20):
        mcSubscribeWithData(ANY_MOTOR, EV_TRACE, &self.event_id,
            pyTraceCallback, <void *>self)
        self.id = mcTraceSubscribeRemote(ANY_MOTOR, level, 0)

    def __dealloc__(self):
        mcTraceUnsubscribeRemote(ANY_MOTOR, self.id)
        mcUnsubscribe(ANY_MOTOR, self.event_id)

    def add(self, channel):
        cdef String buf = bufferFromString(channel)
        mcTraceSubscribeAdd(ANY_MOTOR, self.id, &buf)

    def remove(self, channel):
        cdef String buf = bufferFromString(channel)
        mcTraceSubscribeRemove(ANY_MOTOR, self.id, &buf)

    def emit(self, level, channel, text):
        print("{0}: {1}".format(channel, text))
