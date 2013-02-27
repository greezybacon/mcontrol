from __future__ import print_function

import sys, glob
sys.path.insert(0, '../cli')
for lib in glob.glob('../build/lib.linux-*-{0}.{1}'.format(
        sys.version_info.major, sys.version_info.minor)):
    sys.path.insert(0, lib)

from mcontrol import *
f=Motor('mdrive:///dev/ttyS0@115200:a')
def stall(ev):
    if ev.data.stalled:
        print("Motor stalled")

ev = f.on(Event.EV_MOTION)
ev.call(stall)
f.units=all_units['mil']
f.scale=1e6

f.move(14)

ev.wait()
