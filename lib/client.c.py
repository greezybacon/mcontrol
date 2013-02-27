from __future__ import print_function

import re
proxy = re.compile(r'(?P<flags>IMPORTANT|SLOW)? *PROXYSTUB\((?P<ret>[^,)]+),'
                   r'\s*(?P<name>[^,)]+)'
                   r'(?P<args>(?:,[^,)]+)*)\);', re.M)

def indent(s, tabs):
    lines = s.splitlines()
    first = lines.pop(0)
    return '\n'.join([first] + [
        tabs * 4 * ' ' + line for line in lines])

import sys
def trim(docstring):
    if not docstring:
        return ''
    # Convert tabs to spaces (following the normal Python rules)
    # and split into a list of lines:
    lines = docstring.expandtabs().splitlines()
    # Determine minimum indentation (first line doesn't count):
    indent = sys.maxint
    for line in lines[1:]:
        stripped = line.lstrip()
        if stripped:
            indent = min(indent, len(line) - len(stripped))
    # Remove indentation (first line is special):
    trimmed = [lines[0].strip()]
    if indent < sys.maxint:
        for line in lines[1:]:
            trimmed.append(line[indent:].rstrip())
    # Strip off trailing and leading blank lines:
    while trimmed and not trimmed[-1]:
        trimmed.pop()
    while trimmed and not trimmed[0]:
        trimmed.pop(0)
    # Return a single string:
    return '\n'.join(trimmed)

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

#include <assert.h>
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
        motor_arg = None
        if len(args):
            remote_args = [x for x in args if 'OUT ' in x]
            arg_names = [x.split()[-1] for x in args]
            pointers = [x.split()[-1] for x in args if "*" in x]
            for x in args:
                if 'MOTOR ' in x:
                    motor_arg = x
        else:
            pointers = []
            arg_names = []
            remote_args = []
        items['motor_arg'] = motor_arg
        items['motor_arg_name'] = motor_arg.split()[-1] if motor_arg else ''
        items['arg_names'] = ','.join([x.split()[-1] for x in args])
        items['args'] = ','.join(args)

        items['remote_args'] = remote_args
        items['unpacked_args'] = ",\n        ".join(
                [".{0} = {1}{0}".format(x, "*" if x in pointers else "")
                for x in arg_names]) or "0"

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

class ProxyStubGenerator(object):

    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

    def __getitem__(self, name):
        return self.__dict__[name]

    @property
    def args(self):
        args = self['args'].split(',')
        args = [x.strip() for x in args]
        return args

    @property
    def motor_arg_name(self):
        for arg in self.args:
            parts = arg.split()
            if 'MOTOR' in parts:
                return parts[-1]
        else:
            return ''

    @property
    def has_motor_arg(self):
        for arg in self.args:
            parts = arg.split()
            if 'MOTOR' in parts and not 'OUT' in parts:
                return True
        else:
            return False

    @property
    def client_args(self):
        args = []
        for arg in self.args:
            parts = arg.split()
            if 'MOTOR' in parts:
                parts.remove('MOTOR')
                if 'OUT' in parts:
                    parts.insert(-2, 'int')
                else:
                    parts.insert(-1, 'int')
            args.append(' '.join(parts))
        return ", ".join(args)

    @property
    def stub_call(self):
        args = []
        for arg in self.args:
            parts = arg.split()
            args.append("{0}args->{1}".format(
                '&' if '*' in parts else '', parts[-1]))
        return ", ".join(args)

    @property
    def unpack_motor_inproc(self):
        if self.has_motor_arg:
            return indent(trim("""
            CONTEXT->motor = find_motor_by_id({s.motor_arg_name},
                 CONTEXT->caller_pid);
            if (CONTEXT->motor == NULL)
                return EINVAL;
            """).format(s=self), 2)
        else:
            return ""
    @property
    def impl_call(self):
        args = []
        for arg in self.args:
            args.append(arg.split()[-1])
        return ", ".join(args)

    @property
    def motor_arg(self):
        if self.has_motor_arg:
            return self.motor_arg_name
        else:
            return 0;

    @property
    def unpack_motor(self):
        if self.has_motor_arg:
            return indent(trim("""
            CONTEXT->motor = find_motor_by_id(args->{s.motor_arg_name},
                CONTEXT->caller_pid);
            if (CONTEXT->motor == NULL)
                RETURN (EINVAL);
            """).format(s=self), 2)
        else:
            return ""

    @property
    def stub(self):
        return trim("""
        void {s.name}Stub(request_message_t * message) {{
            UNPACK_ARGS({s.name}, args);
            struct call_context context = {{
                .outofproc = true,
                .caller_pid = message->pid,
            }}, * CONTEXT = &context;
            {s.unpack_motor}
            RETURN ({s.name}Impl(CONTEXT, {s.stub_call}));
        }}
        """).format(s=self)

    @property
    def proxy(self):
        return trim("""
        {s.ret} {s.name}Proxy({s.client_args}) {{
            {s.null_pointer_checks}
            struct _{s.name}_args args = {{
                {s.unpacked_args}
            }};
            response_message_t response;
            struct timespec * waittime = {s.timeout};
            int size = mcMessageSendWait({s.motor_arg}, TYPE_{s.name}, &args,
                sizeof args, {s.priority}, &response, waittime);

            switch (size) {{
                case -ENOENT:
                case -EAGAIN:
                case -EBADF:
                    return -ER_NO_DAEMON;
                case -ETIMEDOUT:
                    return -ER_DAEMON_BUSY;
            }}
            assert(size > 0);

            struct _{s.name}_args * payload =
                (struct _{s.name}_args *)(char *) response.payload;

            {s.unpacked_rets}
            return payload->returned;
        }}
        """).format(s=self)

    @property
    def trampoline(self):
        return trim("""
        {s.ret} {s.name}({s.client_args}) {{
            if (call_mode == MC_CALL_CROSS_PROCESS)
                return {s.name}Proxy({s.arg_names});
            else {{
                {s.null_pointer_checks}
                struct call_context context = {{
                    .inproc = true,
                    .caller_pid = getpid(),
                }}, * CONTEXT = &context;
                {s.unpack_motor_inproc}
                return {s.name}Impl(CONTEXT, {s.impl_call});
            }}
        }}
        """).format(s=self)

# Export function block for each proxy function
for name in sorted(proxies):
    items = proxies[name]
    print("""
{g.stub}
{g.proxy}
{g.trampoline}
""".format(g=ProxyStubGenerator(**items)))
