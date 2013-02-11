from __future__ import print_function

from compile import parse

import sys

p = parse.Parser(environ={'DEBUG': False})

p.parse(*sys.argv[1:])
p.compile()

# Send declarations
p.compose(sys.stdout, declarations=True)

# Enter programming mode
if not p.has_program_entry():
    print("PG 100")

# Send microcode
p.compose(sys.stdout, declarations=False)

# Exit
if not p.has_program_entry():
    print("PG")
