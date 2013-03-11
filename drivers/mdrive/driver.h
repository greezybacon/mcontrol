#ifndef MDRIVE_DRIVER_H
#define MDRIVE_DRIVER_H

#include "mdrive.h"

struct mdrive_reboot_opts {
    // Change to new baudrate at reboot time. After the reboot is sent, the
    // baudrate on the device will be changed to this value (human value) if
    // set to a nonzero value
    int         baudrate;
    // Forgo sending ESC to the motors to halt them
    bool        no_halt;
};

extern int
mdrive_reboot(mdrive_device_t *, struct mdrive_reboot_opts *);

#endif
