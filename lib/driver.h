#ifndef LIBDRIVER_H
#define LIBDRIVER_H

#include "../drivers/driver.h"

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
      _a < _b ? _a : _b; })

typedef struct driver_list driver_info_t;
struct driver_list {
    char *              name;
    DriverClass *       driver;         // XXX: Name driver_class?

    driver_info_t *     head;
    driver_info_t *     next;
};

typedef struct driver_instance driver_instance_t;
struct driver_instance {
    Driver *            driver;
    char                cxn_string[64];

    driver_instance_t * head;
    driver_instance_t * next;
};

extern int
mcDriverSearch(DriverClass * class, char * results, int size);

extern DriverClass *
mcDriverLookup(const char * name);

extern void
mcDriverLoad(const char * path);

extern int
mcDriverConnect(const char * cxn_string, struct driver_instance ** instance);

extern void
mcDriverDisconnect(struct driver_instance * instance);

extern Driver *
find_driver_by_id(int);

extern void *
mcEnumDrivers(void);

extern Driver *
mcEnumDriversNext(void ** enum_id);

extern void
mcDriverCacheInvalidate(Driver * driver);

#endif
