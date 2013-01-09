#ifndef FIRMWARE_H
#define FIRMWARE_H

#include "message.h"
#include "motor.h"

SLOW PROXYDEF(mcFirmwareLoad, int, String * filename);
SLOW PROXYDEF(mcMicrocodeLoad, int, String * filename);

#endif
