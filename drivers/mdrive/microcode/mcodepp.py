from __future__ import print_function

from compile import parse

import sys

p = parse.Parser(environ={'DEBUG': True})

p.parse(*sys.argv[1:])
p.compile()

# Send declarations
p.compose(sys.stdout, declarations=True)

# Enter programming mode
print("PG 50")

# Send microcode
p.compose(sys.stdout, declarations=False)

# Exit
print("PG")
