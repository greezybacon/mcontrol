#ifndef FIRMWARE_H
#define FIRMWARE_H

#include "message.h"
#include "motor.h"

PROXYDEF(mcFirmwareLoad, int, String * filename);
PROXYDEF(mcMicrocodeLoad, int, String * filename);

#endif
