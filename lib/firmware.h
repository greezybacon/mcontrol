#ifndef FIRMWARE_H
#define FIRMWARE_H

#include "message.h"
#include "motor.h"

SLOW PROXYSTUB(int, mcFirmwareLoad, MOTOR motor, String * filename);
SLOW PROXYSTUB(int, mcMicrocodeLoad, MOTOR motor, String * filename);

#endif
