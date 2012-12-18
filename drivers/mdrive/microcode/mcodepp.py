from __future__ import print_function

from compile import parse

import sys

p = parse.Parser()

p.parse(*sys.argv[1:])
p.compile()

p.compose(sys.stdout)
