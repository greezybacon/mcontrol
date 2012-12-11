#ifndef PROFILE_H
#define PROFILE_H

#include "message.h"

PROXYDEF(mcProfileGet, int, OUT Profile * profile);
PROXYDEF(mcProfileSet, int, Profile * profile);

PROXYDEF(mcProfileGetAccel, int, Profile * profile, unit_type_t units,
    OUT int * value);
PROXYDEF(mcProfileGetDecel, int, Profile * profile, unit_type_t units,
    OUT int * value);
PROXYDEF(mcProfileGetInitialV, int, Profile * profile, unit_type_t units,
    OUT int * value);
PROXYDEF(mcProfileGetMaxV, int, Profile * profile, unit_type_t units,
    OUT int * value);
PROXYDEF(mcProfileGetMaxSlip, int, Profile * profile, unit_type_t units,
    OUT int * value);
PROXYDEF(mcProfileGetDeadband, int, Profile * profile, unit_type_t units,
    OUT int * value);

#endif
