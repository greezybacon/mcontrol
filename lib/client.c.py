from __future__ import print_function

import re
proxy = re.compile(r'(?P<flags>IMPORTANT|SLOW)? *PROXYDEF\((?P<name>[^,)]+),'
                   r'\s*(?P<ret>[^,)]+)'
                   r'(?P<args>(?:,[^,)]+)*)\);', re.M)

proxies = {}

def unpack_return(arg):
    name = arg.split()[-1]
    type = " ".join(arg.split()[1:-1])
    if '*' in type:
        return "memcpy({0}, &payload->{0}, sizeof({1}));" \
            .format(name, type.replace("*","").strip())
    else:
        return "{0} = payload->{0};".format(name)

print("""
/**
 ** Automatically generated file -- do not modify directly
 **
 ** This file is generated by client.c.py which will compile the list of
 ** exported proxy functions from the given header files.
 **
 **/

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../drivers/driver.h"
#include "../motor.h"
#include "message.h"
#include "dispatch.h"
#include "client.h"

// Default timeout for send/receive is defined here. The time should
// consider the send, remote process, remote send, and receive times
// together.
static struct timespec timeout = { .tv_nsec = 250e6 }; // 250ms

void
mcClientTimeoutSet(const struct timespec * new_timeout,
        struct timespec * old_timeout) {
    if (old_timeout != NULL)
        *old_timeout = timeout;
    timeout = *new_timeout;
}

static enum mcCallMode call_mode = MC_CALL_CROSS_PROCESS;

void
mcClientCallModeSet(enum mcCallMode mode) {
    call_mode = mode;
}

enum mcCallMode
mcClientCallModeGet(void) {
    return call_mode;
}
""")

import sys
for doth in sys.argv:
    if doth == "message.h" or '.py' in doth:
        continue
    print('#include "{0}"'.format(doth))
    for stub in proxy.finditer(open(doth, 'rt').read()):
        items = stub.groupdict()
        args = items['args'].split(',')[1:]
        motor_arg = "motor_t motor"
        if len(args):
            remote_args = [x for x in args if 'OUT ' in x]
            args = [x.replace('OUT ','') for x in args]
            arg_names = [x.split()[-1] for x in args]
            pointers = [x.split()[-1] for x in args if "*" in x]
            for x in args:
                if 'MOTOR ' in x:
                    motor_arg = ""
        else:
            pointers = []
            arg_names = []
            remote_args = []
        items['motor_arg'] = motor_arg
        items['motor_arg_name'] = motor_arg.split()[-1] if motor_arg else ''
        items['arg_names'] = ','.join([x.split()[-1] for x in args])
        if not len(motor_arg) and len(args):
            # Drop leading comma from args list
            items['args'] = items['args'].strip()[1:]
        elif len(motor_arg) and len(args):
            items['motor_arg_name'] += ','

        items['remote_args'] = remote_args
        items['unpacked_args'] = ",\n        ".join(
                [".{0} = {1}{0}".format(x, "*" if x in pointers else "")
                for x in arg_names])

        # Check pointer arguments for NULL
        items['null_pointer_checks'] = "\n    ".join(
            "if ({0} == NULL) return -EINVAL;".format(x)
            for x in pointers
        )
        items['unpacked_rets'] = "\n    ".join(
                unpack_return(x) for x in remote_args)

        # Deal with high-priority messages
        if items['flags'] and 'IMPORTANT' in items['flags']:
            items['priority'] = 'PRIORITY_HIGH'
        else:
            items['priority'] = 'PRIORITY_CMD'

        if items['flags'] and 'SLOW' in items['flags']:
            items['timeout'] = 'NULL'
        else:
            items['timeout'] = '&timeout'

        proxies[items['name']] = items


# Export function block for each proxy function
for name in sorted(proxies):
    items = proxies[name]
    print("""
{ret} {name}OutOfProc({motor_arg}{args}) {{
    {null_pointer_checks}
    struct _{name}_args args = {{
        .outofproc = true,
        {unpacked_args}
    }};
    response_message_t response;
    struct timespec * waittime = {timeout};
    int size = mcMessageSendWait(motor, TYPE_{name}, &args,
        sizeof args, {priority}, &response, waittime);

    switch (size) {{
        case -ENOENT:
        case -EAGAIN:
            return -ER_NO_DAEMON;
        case -ETIMEDOUT:
            return -ER_DAEMON_BUSY;
    }}

    struct _{name}_args * payload = (struct _{name}_args *)(char *) response.payload;

    {unpacked_rets}
    return payload->returned;
}}

{ret} {name}InProc({motor_arg}{args}) {{
    request_message_t message = {{
        .id = 0,
        .pid = getpid(),
        .motor_id = motor,
    }};
    struct _{name}_args args = {{
        .inproc = true,
        {unpacked_args}
    }};
    *(struct _{name}_args *)message.payload = args;
    {name}Impl(&message);
    struct _{name}_args * payload = (struct _{name}_args *)(char *) message.payload;
    {unpacked_rets}
    return payload->returned;
}}

{ret} {name}({motor_arg}{args}) {{
    if (call_mode == MC_CALL_CROSS_PROCESS)
        return {name}OutOfProc({motor_arg_name}{arg_names});
    else
        return {name}InProc({motor_arg_name}{arg_names});
}}
""".format(**items))
