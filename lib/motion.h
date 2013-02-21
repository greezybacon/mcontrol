#ifndef MOTION_H
#define MOTION_H

#include "message.h"
#include "motor.h"

PROXYSTUB(int, mcMoveAbsolute, MOTOR motor, int distance);
PROXYSTUB(int, mcMoveAbsoluteUnits, MOTOR motor, int distance, unit_type_t units);
PROXYSTUB(int, mcMoveRelative, MOTOR motor, int distance);
PROXYSTUB(int, mcMoveRelativeUnits, MOTOR motor, int distance, unit_type_t units);

PROXYSTUB(int, mcSlew, MOTOR motor, int rate);
PROXYSTUB(int, mcSlewUnits, MOTOR motor, int rate, unit_type_t units);

IMPORTANT PROXYSTUB(int, mcStop, MOTOR motor);
IMPORTANT PROXYSTUB(int, mcHalt, MOTOR motor);
IMPORTANT PROXYSTUB(int, mcEStop, MOTOR motor);

// XXX: What about profile (slew rate, etc)
PROXYSTUB(int, mcHome, MOTOR motor, enum home_type type, enum home_direction direction);

PROXYSTUB(int, mcUnitScaleSet, MOTOR motor, unit_type_t units, long long urevs);
PROXYSTUB(int, mcUnitScaleGet, MOTOR motor, OUT unit_type_t * units,
    OUT long long * urevs);
PROXYSTUB(int, mcUnitToMicroRevs, MOTOR motor, unit_type_t unit,
    OUT long long * urevs);

extern int mcDistanceToMicroRevs(Motor * motor, int distance,
    long long * urevs);
extern int mcDistanceUnitsToMicroRevs(Motor * motor, int distance,
    unit_type_t units, long long * urevs);
extern int mcMicroRevsToDistance(Motor * motor, long long urevs,
    int * distance);
extern int mcMicroRevsToDistanceUnits(Motor * motor, long long urevs,
    int * distance, unit_type_t units);

#endif
