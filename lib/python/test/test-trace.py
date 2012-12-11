from __future__ import print_function

import sys, glob
version = sys.version_info
build_dir = glob.glob("../build/lib*{0}.{1}".format(
    version.major, version.minor))
sys.path.insert(0, build_dir[0])

from mcontrol import *
f=Motor('mdrive:///dev/ttyS0:b')
def ptrace(level, source, buffer):
 print('(%d): %s: %s' % (level, source, buffer))

trace_on(ptrace)
f.units=MILLI_INCH
f.move(13)
