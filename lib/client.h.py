from __future__ import print_function

import re
proxy = re.compile(r'PROXYDEF\((?P<name>[^,)]+),'
                   r'\s*(?P<ret>[^,)]+)'
                   r'(?P<args>(?:,[^,)]+)*)\);', re.M)

proxies = {}

def unpack_return(arg):
    name = arg.split()[-1]
    type = " ".join(arg.split()[1:-1])
    if '*' in type:
        return "memcpy({0}, &payload.{0}, sizeof({1}));" \
            .format(name, type.replace("*","").strip())
    else:
        return "{0} = payload.{0};".format(name)

print("""
/**
 ** Automatically generated file -- do not modify directly
 **
 ** This file is generated by client.h.py which will compile the list of
 ** exported proxy functions from the given header files.
 **
 **/

#include <stdbool.h>
#include <stdlib.h>

#include "../drivers/driver.h"
#include "../motor.h"
#include "message.h"

extern void mcClientTimeoutSet(const struct timespec *, struct timespec *);
""")

import sys
for doth in sys.argv:
    if doth == "message.h" or '.py' in doth:
        continue
    print('#include "{0}"'.format(doth))
    for stub in proxy.finditer(open(doth, 'rt').read()):
        items = stub.groupdict()
        args = items['args'].split(',')[1:]
        items['motor_arg'] = "motor_t motor"
        if len(args):
            args = [x.replace('OUT ','') for x in args]
            args = [x.replace("*","") for x in args]
            for x in args:
                if 'MOTOR ' in x:
                    # MOTOR argument is specified in the parameter list
                    items['motor_arg'] = ""
        if items['motor_arg'] == "" and len(args):
            # Drop leading ',' form args
            items['args'] = items['args'].strip()[1:]
        items['args_no_pointer'] = "\n    ".join(
                 "{0};".format(x.strip()) for x in args)
        proxies[items['name']] = items

# Export function def and args struct for each proxy function
for name in sorted(proxies):
    items = proxies[name]
    print("""
extern %(ret)s %(name)s(%(motor_arg)s%(args)s);

struct _%(name)s_args {
    bool inproc;        // If the call is made in-process (server-server)
    bool outofproc;     // If the call is made out-of-process (client-server)
    %(ret)s returned;
    %(args_no_pointer)s
};""" % items)
