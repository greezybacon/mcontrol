#include "firmware.h"

#include "client.h"
#include "motor.h"

#include <errno.h>

int
PROXYIMPL(mcFirmwareLoad, MOTOR motor, String * filename) {

    if (!SUPPORTED(CONTEXT->motor, load_firmware))
        return ENOTSUP;

    return INVOKE(CONTEXT->motor, load_firmware, filename->buffer);
}

int
PROXYIMPL(mcMicrocodeLoad, MOTOR motor, String * filename) {

    if (!SUPPORTED(CONTEXT->motor, load_microcode))
        return ENOTSUP;

    return INVOKE(CONTEXT->motor, load_microcode, filename->buffer);
}
