from __future__ import print_function

import re, sys
proxy = re.compile(r'PROXYDEF\((?P<name>[^,)]+),'
                   r'\s*(?P<ret>[^,)]+)'
                   r'(?P<args>(?:,[^,)]+)*)\);', re.M)

funcs = []
for doth in sys.argv:
    for stub in proxy.finditer(open(doth, 'rt').read()):
        items = stub.groupdict()
        funcs.append(items['name'])

# Export enum of all proxy functions (to create ID numbers)

print("""enum proxy_function_ids {
    TYPE__FIRST = 100,""")
for name in sorted(funcs):
    print("    TYPE_%s," % name)
print( """    TYPE__LAST
};
""")

# Export defs for Impl functions

for i in sorted(funcs):
    print("extern void %sImpl(request_message_t * message);" % (i,))

# Export dispatch table (list of message type numbers and corresponding
# functions

print("""
#ifdef DISPATCH_INCLUDE_TABLE
struct dispatch_table {
    int type;
    void (*callable)(request_message_t * message);
    const char * name;
} table[] = {""")

for i in sorted(funcs):
    print('    {{ TYPE_{0}, {0}Impl, "{0}" }},'.format(i))

print("""    { 0, NULL, NULL }
};
#endif
""")
