from __future__ import print_function

import re, sys
proxy = re.compile(r'(?P<flags>IMPORTANT|SLOW)? *PROXYSTUB\((?P<ret>[^,)]+),'
                   r'\s*(?P<name>[^,)]+)'
                   r'(?P<args>(?:,[^,)]+)*)\);', re.M)

funcs = []
for doth in sys.argv[1:]:
    print('#include "{0}"'.format(doth))
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

# Export dispatch table (list of message type numbers and corresponding
# functions

print("""
#ifdef DISPATCH_INCLUDE_TABLE
struct dispatch_table {
    int type;
    void (*callable)(request_message_t * message);
} table[] = {""")

for i in sorted(funcs):
    print("    { TYPE_%s, %sStub }," % (i, i))

print("""    { 0, NULL }
};
#endif
""")
