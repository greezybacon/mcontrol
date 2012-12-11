#include "../drivers/driver.h"

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
      _a < _b ? _a : _b; })

extern int
mcDriverSearch(DriverClass * class, char * results, int size);

extern DriverClass *
mcDriverLookup(const char * name);

extern void
mcDriverLoad(const char * path);

extern int
mcDriverConnect(const char * cxn_string, Driver ** driver);

extern void
mcDriverDisconnect(Driver *);

extern Driver *
find_driver_by_id(int);

extern void *
mcEnumDrivers(void);

extern Driver *
mcEnumDriversNext(void ** enum_id);
