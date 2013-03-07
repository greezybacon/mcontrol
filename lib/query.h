#ifndef LIB_QUERY_H
#define LIB_QUERY_H

#include "message.h"

PROXYSTUB(int, mcQueryInteger, MOTOR motor, motor_query_t query,
    OUT int * value);
PROXYSTUB(int, mcQueryIntegerUnits, MOTOR motor, motor_query_t query,
    OUT int * value, unit_type_t units);
PROXYSTUB(int, mcQueryIntegerWithStringItem, MOTOR motor, motor_query_t query,
    OUT int * value, String * item);
PROXYSTUB(int, mcQueryIntegerWithIntegerItem, MOTOR motor, motor_query_t query,
    OUT int * value, int item);


PROXYSTUB(int, mcQueryFloat, MOTOR motor, motor_query_t query,
    OUT double * value);
PROXYSTUB(int, mcQueryFloatUnits, MOTOR motor, motor_query_t query,
    OUT double * value, unit_type_t units);

PROXYSTUB(int, mcQueryString, MOTOR motor, motor_query_t query,
    OUT String * value);

PROXYSTUB(int, mcPokeString, MOTOR motor, motor_query_t query,
    String * value);
SLOW PROXYSTUB(int, mcPokeStringWithStringItem, MOTOR motor,
    motor_query_t query, String * value, String * item);
SLOW PROXYSTUB(int, mcPokeInteger, MOTOR motor, motor_query_t query, int value);
PROXYSTUB(int, mcPokeIntegerUnits, MOTOR motor, motor_query_t query, int value,
    unit_type_t units);
PROXYSTUB(int, mcPokeIntegerWithStringItem, MOTOR motor, motor_query_t query,
    int value, String * item);
PROXYSTUB(int, mcPokeIntegerWithIntegerItem, MOTOR motor, motor_query_t query,
    int value, int item);

#endif
