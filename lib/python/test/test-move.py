from __future__ import print_function

from mcontrol import *
f=Motor('mdrive:///dev/ttyS0')
def stall(ev):
 print("Motor stalled")

ev = f.on(EV_MOTOR_STALLED, stall)
f.units=MILLI_INCH
f.move(1)

ev.wait()
