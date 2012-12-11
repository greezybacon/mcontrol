from mcontrol.defs cimport *

cdef int ANY_MOTOR = -1

import logging

cdef void pyTraceCallback(event_data_t * data):
    if data.event != EV_TRACE:
        # Spurious event
        return

    cdef bytes text = data.event_data.trace.buffer

    assert data.user_data != NULL
    (<object>data.user_data).emit(
        data.event_data.trace.level,
        data.event_data.trace.channel, text.decode('latin-1'))

cdef class Trace:
    cdef int id

    def __init__(self, level=20):
        mcSubscribeWithData(ANY_MOTOR, EV_TRACE, pyTraceCallback,
            <void *>self)
        self.id = mcTraceSubscribeRemote(ANY_MOTOR, level, 0)

    def __dealloc__(self):
        mcTraceUnsubscribeRemote(ANY_MOTOR, self.id)
        mcUnsubscribe(ANY_MOTOR, EV_TRACE, pyTraceCallback)

    def add(self, channel):
        cdef String buf = bufferFromString(channel)
        mcTraceSubscribeAdd(ANY_MOTOR, self.id, &buf)

    def remove(self, channel):
        cdef String buf = bufferFromString(channel)
        mcTraceSubscribeRemove(ANY_MOTOR, self.id, &buf)

    def emit(self, level, channel, text):
        print("{0}: {1}".format(channel, text))
