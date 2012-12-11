from libc.errno cimport EINVAL

from mcontrol.defs cimport *

def raise_status(status, message):
    errors = {
        ER_NO_DAEMON: NoDaemonException,
        -ER_NO_DAEMON: NoDaemonException,
        EINVAL: ValueError,
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

include "motor.pyx"
include "trace.pyx"
include "profile.pyx"
