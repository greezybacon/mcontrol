
PROXYDEF(mcQueryInteger, int, motor_query_t query, OUT int * value);
PROXYDEF(mcQueryIntegerUnits, int, motor_query_t query, OUT int * value,
    unit_type_t units);
PROXYDEF(mcQueryIntegerItem, int, motor_query_t query, OUT int * value,
    String * item);

PROXYDEF(mcQueryFloat, int, motor_query_t query, OUT double * value);
PROXYDEF(mcQueryFloatUnits, int, motor_query_t query, OUT double * value,
    unit_type_t units);

PROXYDEF(mcQueryString, int, motor_query_t query, OUT String * value);

PROXYDEF(mcPokeString, int, motor_query_t query, String * value);
PROXYDEF(mcPokeStringItem, int, motor_query_t query, String * value,
    String * item);
PROXYDEF(mcPokeInteger, int, motor_query_t query, int value);
PROXYDEF(mcPokeIntegerUnits, int, motor_query_t query, int value,
    unit_type_t units);
PROXYDEF(mcPokeIntegerItem, int, motor_query_t query, int value,
    String * item);
