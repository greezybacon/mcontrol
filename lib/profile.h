#ifndef PROFILE_H
#define PROFILE_H

#include "message.h"

PROXYSTUB(int, mcProfileGet, MOTOR motor, OUT Profile * profile);
PROXYSTUB(int, mcProfileSet, MOTOR motor, Profile * profile);

PROXYSTUB(int, mcProfileGetAccel, MOTOR motor, Profile * profile, unit_type_t units,
    OUT int * value);
PROXYSTUB(int, mcProfileGetDecel, MOTOR motor, Profile * profile, unit_type_t units,
    OUT int * value);
PROXYSTUB(int, mcProfileGetInitialV, MOTOR motor, Profile * profile, unit_type_t units,
    OUT int * value);
PROXYSTUB(int, mcProfileGetMaxV, MOTOR motor, Profile * profile, unit_type_t units,
    OUT int * value);
PROXYSTUB(int, mcProfileGetMaxSlip, MOTOR motor, Profile * profile, unit_type_t units,
    OUT int * value);
PROXYSTUB(int, mcProfileGetDeadband, MOTOR motor, Profile * profile, unit_type_t units,
    OUT int * value);

#endif
