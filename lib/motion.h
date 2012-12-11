#ifndef MOTION_H
#define MOTION_H

#include "message.h"
#include "motor.h"

PROXYDEF(mcMoveAbsolute, int, int distance);
PROXYDEF(mcMoveAbsoluteUnits, int, int distance, unit_type_t units);
PROXYDEF(mcMoveRelative, int, int distance);
PROXYDEF(mcMoveRelativeUnits, int, int distance, unit_type_t units);

IMPORTANT PROXYDEF(mcStop, int);

/*
PROXYDEF(mcHome, int);
PROXYDEF(mcJitter, int, int loops, int max_travel);
PROXYDEF(mcJitterUnits, int, int loops, int max_travel, unit_type_t units);
*/

PROXYDEF(mcUnitScaleSet, int, unit_type_t units, long long urevs);
PROXYDEF(mcUnitScaleGet, int, OUT unit_type_t * units, OUT long long * urevs);
PROXYDEF(mcUnitToMicroRevs, int, unit_type_t unit, OUT long long * urevs);

extern int mcDistanceToMicroRevs(Motor * motor, int distance,
    long long * urevs);
extern int mcDistanceUnitsToMicroRevs(Motor * motor, int distance,
    unit_type_t units, long long * urevs);
extern int mcMicroRevsToDistance(Motor * motor, long long urevs,
    int * distance);
extern int mcMicroRevsToDistanceUnits(Motor * motor, long long urevs,
    int * distance, unit_type_t units);

#endif
