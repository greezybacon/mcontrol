#include "firmware.h"

#include "client.h"
#include "motor.h"

#include <errno.h>

PROXYIMPL(mcFirmwareLoad, String filename) {
    UNPACK_ARGS(mcFirmwareLoad, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->load_firmware == NULL)
        RETURN( ENOTSUP );

    RETURN( m->driver->class->load_firmware(m->driver,
        args->filename.buffer) );
}

PROXYIMPL(mcMicrocodeLoad, String filename) {
    UNPACK_ARGS(mcMicrocodeLoad, args);

    Motor * m = find_motor_by_id(motor, message->pid);
    if (m == NULL)
        RETURN( EINVAL );

    if (m->driver->class->load_microcode == NULL)
        RETURN( ENOTSUP );

    RETURN( m->driver->class->load_microcode(m->driver,
        args->filename.buffer) );
}
