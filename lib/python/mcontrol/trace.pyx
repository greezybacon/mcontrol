from mcontrol.defs cimport *

cdef int ANY_MOTOR = -1

import logging

cdef void pyTraceEventCallback(event_info_t * info) with gil:
    if info.event != EV_TRACE:
        # Spurious event
        return

    cdef bytes text = info.data.trace.buffer

    assert info.user != NULL
    (<object>info.user).emit(
        info.data.trace.level,
        info.data.trace.channel, text.decode('latin-1'))

trace_instances = {}
cdef void pyTraceCallback(int id, int level, int channel,
        char * buffer) with gil:
    PyEval_InitThreads()
    if id in trace_instances:
        trace_instances[id].emit(level, channel, buffer.decode('latin-1'))

cdef class Trace(object):
    cdef int id
    cdef int event_id

    def __cinit__(self, level=40):
        if not Library.is_in_process():
            mcSubscribeWithData(ANY_MOTOR, EV_TRACE, &self.event_id,
                pyTraceEventCallback, <void *>self)
            self.id = mcTraceSubscribeRemote(ANY_MOTOR, level, 0)
        else:
            self.id = mcTraceSubscribe(level, 0, <trace_callback_t>pyTraceCallback)
        trace_instances[self.id] = self

    def __dealloc__(self):
        if self.id:
            del trace_instances[self.id]
            if not Library.is_in_process():
                mcTraceUnsubscribeRemote(ANY_MOTOR, self.id)
                mcUnsubscribe(ANY_MOTOR, self.event_id)
            else:
                mcTraceUnsubscribe(self.id)

    def add(self, channel, level=20):
        cdef String buf = bufferFromString(channel)
        status = mcTraceSubscribeAdd(ANY_MOTOR, self.id, &buf)
        if status < 0:
            raise ValueError('{0}: No such channel'.format(channel))

    def remove(self, channel):
        cdef String buf = bufferFromString(channel)
        mcTraceSubscribeRemove(ANY_MOTOR, self.id, &buf)

    def emit(self, level, channel, text):
        print("{0}: {1}".format(channel, text))
