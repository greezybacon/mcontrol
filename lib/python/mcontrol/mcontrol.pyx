from libc.errno cimport EINVAL, EIO

from mcontrol.defs cimport *

def raise_status(status, message):
    errors = {
        ER_NO_DAEMON: NoDaemonException,
        -ER_NO_DAEMON: NoDaemonException,
        EINVAL: ValueError,
        EIO: IOError,
        ER_COMM_FAIL: CommFailure,
        ER_DAEMON_BUSY: NoDaemonException,
        -ER_DAEMON_BUSY: NoDaemonException
    }

    if status == 0:
        return

    if status in errors:
        raise errors[status](message)
    else:
        raise RuntimeError("{0}: {1}".format(status, message))

cdef extern from "lib/client.h" nogil:
    enum mcCallMode:
        MC_CALL_IN_PROCESS
        MC_CALL_CROSS_PROCESS

cdef class Library:
    @classmethod
    def run_in_process(cls):
        # Set call mode to in-process (not out-of-process)
        if not cls.is_in_process:
            cls.crash_cleanly()
        mcClientCallModeSet(MC_CALL_IN_PROCESS)

    @classmethod
    def run_out_of_process(cls):
        mcClientCallModeSet(MC_CALL_CROSS_PROCESS)

    @classmethod
    def is_in_process(cls):
        return mcClientCallModeGet() == MC_CALL_IN_PROCESS

    @classmethod
    def load_driver(cls, name):
        import os
        if not os.path.exists(name):
            raise ValueError("{0}: Driver library does not exist"
                .format(name))
        name = name.encode('latin-1')
        cdef char * cstring = <char *>name
        with nogil:
            mcDriverLoad(cstring)

    cdef void cleanup_signal_handler(Library cls, int signal):
        exit(0);

    # Exit cleanly
    @classmethod
    def crash_cleanly(cls, name):
        import signal
        signal.signal(signal.SIGHUP, cls.cleanup_signal_handler)


include "profile.pyx"
include "motor.pyx"
include "trace.pyx"
