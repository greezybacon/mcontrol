#include "driver.h"

#include "events.h"
#include "trace.h"

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static struct driver_list * drivers = NULL;
static struct driver_instance * motors = NULL;
static int motor_uid = 1;

/**
 * mcDriverRegister
 *
 * Called (internally) by driver classes when they are loaded. Loading
 * should happen either whenever the shared library is loaded or when the
 * program is started if the driver is dynamically or statically linked to
 * the program that is loading this library (the motor contor library).
 *
 * Parameters:
 * class - (DriverClass *) information about the device driver and links to
 *      the driver functions available to control the device
 */
void
mcDriverRegister(DriverClass * class) {
    if (drivers == NULL) {
        drivers = calloc(1, sizeof *drivers);
    }

    if (class == NULL || class->name == NULL) {
        printf("Programming error: driver must specify .name\n");
        return;
    }
    
    printf("Driver: %s, %s\n", class->description, class->revision);

    driver_info_t * current = drivers;

    // Advance to end of the list
    while (current->next) current = current->next;

    // Add driver to end of list
    if (current->head)
        current = current->next = calloc(1, sizeof *current);

    current->head = drivers;
    current->driver = class;
    current->name = class->name;
}

/**
 * mcDriverSearch
 *
 * Invoke DriverClass::search on the specified driver class. The ::search
 * method should return a list of connection strings packed into the given
 * buffer with a null-byte separator.
 *
 * Returns the number of connection strings detected from the device bus.
 * These connection strings can be passed into driver_connect to receive an
 * instance of driver connected to the motor accessible by the connection
 * string.
 */
int
mcDriverSearch(DriverClass * class, char * results, int size) {
    // TODO: Probe motor driver to find motors
    char * strings = malloc(2048), * head = strings;
    int count1 = class->search(strings, size), count=0, wr;

    while (count1--) {
        wr = snprintf(results, size, "%s://%s", class->name, strings);
        results += wr;
        size -= wr;
        count++;

        if (size < 0)
            break;

        // Advance to next string in the list
        while (*strings++);
    }
    free(head);

    return count;
}

/**
 * mcDriverConnect
 *
 * Called to create and receive an instance of a driver, connected to a
 * motor. This will be the kernel-land equivalent of the user-land Motor
 * type which will be used to perform any operations on a motor
 *
 * Calls to mcDriverConnect with the same connection string will receive the
 * self-same return result. A cached result is returned for multiple calls
 * with matching connection strings.
 *
 * Parameters:
 * cxn_string - (String) connection identifier of the motor to receive a
 *      driver instance for. In the form of driver://conn-string, where
 *      "driver" is the name of a registered driver class, and "conn-string"
 *      is the format of connection strings required by the respective
 *      driver format. See DriverClass::initialize for the respective driver
 *      for the specific format.
 * driver - (struct driver_instance **) pointer to the driver instance which
 *      will contain information about the connection to the device driver.
 *      The first-level pointer will be set by this routine.
 *
 * Returns:
 * (Driver) instance of the connected device
 */
int
mcDriverConnect(const char * cxn_string, struct driver_instance ** instance) {
    static const char * regex = "^([^:]+)://(.+)$";
    static regex_t re_cxn;

    regmatch_t matches[3];

    // Parse connection string
    if (!re_cxn.re_nsub)
        regcomp(&re_cxn, regex, REG_EXTENDED);

    if (regexec(&re_cxn, cxn_string, 3, matches, 0) != 0)
        // Bad connection string
        return EINVAL;

    DriverClass * class;
    char class_name[32];
    snprintf(class_name,
        min(sizeof class_name, matches[1].rm_eo - matches[1].rm_so + 1),
        "%s", cxn_string);
    class = mcDriverLookup(class_name);

    if (class == NULL)
        // No class registered under the name given
        // XXX: Set some kind of error response
        return ENOMEM;

    if (motors == NULL) {
        motors = calloc(1, sizeof *motors);
        motors->head = motors;
    }

    // Return a driver requested by the same connection string
    for (struct driver_instance * m = motors->head; m; m=m->next) {
        if (strncmp(m->cxn_string, cxn_string, sizeof m->cxn_string) == 0
                && strlen(m->cxn_string) == strlen(cxn_string)
                && m->driver) {
            *instance = m;
            return 0;
        }
    }

    // Otherwise, initialize a new driver
    if (motors->driver) {
        motors->next = calloc(1, sizeof *motors);
        motors->next->head = motors->head;
        motors = motors->next;
    }

    motors->driver = calloc(1, sizeof *motors->driver);
    if (motors->driver == NULL)
        return ENOMEM;
    
    motors->driver->id = motor_uid++;
    motors->driver->class = class;

    *instance = motors;

    int status = class->initialize(motors->driver,
            cxn_string + matches[2].rm_so);
    if (status)
        return status;

    // Cache the driver instance with the connection string
    snprintf(motors->cxn_string, sizeof motors->cxn_string,
        "%s", cxn_string);

    return 0;
}

/**
 * mcDriverDisconnect
 *
 * Disconnects a device driver. This might be similar to a *nix hangup
 * signal. The device's destroy() method is called, and then the memory held
 * in this library for the device is freed and the device will then be set
 * to point to NULL.
 */
void
mcDriverDisconnect(struct driver_instance * instance) {
    if (instance == NULL)
        return;

    Driver * driver = instance->driver;
    if (driver == NULL || driver->class == NULL)
        // XXX: Set some kind of error response
        return;

    if (driver->class->destroy)
        driver->class->destroy(driver);

    free(driver);
    instance->driver = NULL;
}

void
mcDriverCacheInvalidate(Driver * driver) {
    // Return a driver requested by the same connection string
    for (struct driver_instance * m = motors->head; m; m=m->next) {
        if (m->driver == driver) {
            bzero(m->cxn_string, sizeof m->cxn_string);
            return;
        }
    }
}

void __attribute__((destructor))
_destroy_motors(void) {
    struct driver_instance * device;
    if (motors)
        device = motors->head;
    else
        return;

    while (device) {
        mcDriverDisconnect(device);
        device = device->next;
    }
}

DriverClass *
mcDriverLookup(const char * name) {
    driver_info_t * current = drivers;
    
    while (current != NULL)
        if (strncmp(current->name, name, sizeof *current->name) == 0)
            return current->driver;

    return NULL;
}

Driver *
find_driver_by_id(int id) {
    for (struct driver_instance * m = motors->head; m; m=m->next)
        if (m->driver->id == id)
            return m->driver;
    return NULL;
}

void *
mcEnumDrivers(void) {
    return motors->head;
}

Driver *
mcEnumDriversNext(void ** enum_id) {
    Driver * retval;

    if (enum_id == NULL || *enum_id == NULL)
        return NULL;

    // XXX: Make this safer

    struct driver_instance * m = *enum_id;
    retval = (Driver *) m->driver;
    *enum_id = m->next;

    return retval;
}

void
mcDriverLoad(const char * path) {
    char modulepath[PATH_MAX];

    if (strchr(path, '/') == NULL)
        snprintf(modulepath, sizeof modulepath, "./%s", path);
    else
        snprintf(modulepath, sizeof modulepath, "%s", path);

    void * module = dlopen(modulepath, RTLD_NOW);

    if (!module)
        printf("Unable to load module: %s\n", dlerror());
}
