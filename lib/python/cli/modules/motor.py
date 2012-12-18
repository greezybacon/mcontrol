from cli import Shell

from mcontrol import all_units, Profile, Event
from mcontrol import NoDaemonException

from lib import term

import time

class MotorContext(Shell):

    intro = None

    def __init__(self, parent, motor, name=""):
        Shell.__init__(self, context=parent.context)
        self.motor = motor
        self.name = name
        self.prompt = Shell.prompt[:-2] + ":{0}> ".format(name)
        self.parent = parent

    def do_get(self, what):
        try:
            if hasattr(self, '_do_get_' + what):
                return getattr(self, '_do_get_' + what)()
            else:
                self.out(repr(getattr(self.motor, what)))
        except NoDaemonException:
            self.error("Daemon not responding", "is one running?")
        except AttributeError:
            self.error("{0}: Unknown attribute".format(what))

    def help_get(self):
        return "Retrieve settings from the unit"

    def do_set(self, what):
        name, val = what.split(' ', 1)
        try:
            if hasattr(self, '_do_set_' + name):
                return getattr(self, '_do_set_' + name)(val)
            else:
                try:
                    setattr(self.motor, name, val)
                except TypeError:
                    setattr(self.motor, name, int(val))
        except NoDaemonException:
            self.error("Daemon not responding", "is one running?")
        except AttributeError:
            self.error("{0}: Unknown attribute".format(what))

    def _do_get_units(self):
        self.status("Motor is configured with units of: "
            + all_units[self.motor.units])

    def _do_set_units(self, what):
        for id, name in all_units.items():
            if name == what:
                self.motor.units = id
                break
        else:
            self.error("{0}: Unsupported units".format(what))

    def _do_set_scale(self, string):
        """
        Set the units and scale of the motor. For example

        scale 0.7 inch

        Means 0.7 inches of travel per revolution
        """
        def usage():
            self.error("Incorrect usage", "see 'help set scale'")

        parts = string.split(' ')
        scale = 1e6 / float(parts.pop(0))
        units = parts.pop(0)
        if len(parts):
            return usage()

        self._do_set_units(units)
        self.motor.scale = float(scale)

    def do_move(self, where):
        func = self.motor.move
        units = None
        wait = False

        parts = where.split()
        if 'wait' in parts:
            wait = True
            parts.remove('wait')

        if parts[0] == 'to':
            func = self.motor.moveTo
            parts.pop(0)
        if parts[-1] in all_units.values():
            for id, name in all_units.items():
                if name == parts[-1]:
                    units = id
                    break
            else:
                self.error("{0}: Unsupported units".format(what))
            parts.pop()
        if len(parts) == 1:
            try:
                func(int(parts[0]), units)
                if wait:
                    self.motor.on(Event.EV_MOTION).wait()
                return
            except:
                raise
        self.error("Incorrect usage", "see 'help move'")

    def _move_done(self, event):
        if event.data.completed:
            term.output("%(BOL)%(BOLD)%(WHITE)"
                ">>> Move completed%(NORMAL)\n",
                self.context['stderr'])
        self.context['stderr'].flush()
        try:
            import readline
            self.context['stderr'].write(self.prompt)
            readline.redisplay()
        except ImportError:
            pass

    def do_slew(self, line):
        """
        Slew the motor at the given rate. Units if not specified are assumed
        to be in the scale units per second

        Example: slew 200 mil
        Slew motor at 200 mils per second
        """
        parts = line.split()
        val = parts.pop(0)
        units = None
        if len(parts):
            for id, name in all_units.items():
                if name == parts[-1]:
                    units = id
                    break
            else:
                self.error("{0}: Unsupported units".format(what))
            parts.pop()
        if len(parts):
            return self.error("Incorrect usage", "see 'help slew")

        self.motor.slew(float(val), units)

    def do_stop(self, line):
        """
        Stop the motor immediately
        """
        self.motor.stop()

    def do_home(self, line):
        """
        Home the motor using the described method and direction. For
        instance,

        home stop left

        Will home the motor to a hard stop in the left hand (negative)
        direction. If homing to hard stop, the position counter will not be
        reset after the device is homed. In that case, follow the command
        with a 'set position 0' or whatever position is reflected by the
        unit at the location of the hard stop.
        """
        parts = line.split()
        # TODO: Collect current unit/scale
        self.motor.units = all_units['milli-rev']
        self.motor.scale = 1000
        try:
            self.motor.encoder = 1
        except:
            raise
        rate = 500
        if 'left' in parts:
            rate = -rate
        if 'stop' in parts:
            # Software implementation of home to hard stop
            self.motor.slew(rate)
            event = self.motor.on(Event.EV_MOTION)
            while self.motor.moving:
                if event.isset:
                    if event.data.stalled:
                        break
                    # Not a stall event, wait for a stall event
                    info.reset()
                time.sleep(0.1)

    def do_info(self, line):
        """
        Display information about the connection motor such a serial and
        part numbers, firmware version, etc. It will also display
        information about the motor's motion details if the profile and
        scale have been setup (see set scale)
        """
        term.output("%(BOLD)%(WHITE)  Serial: %(NORMAL){0}".format(
            self.motor.serial), self.context['stdout'])
        term.output("%(BOLD)%(WHITE)   Model: %(NORMAL){0}".format(
            self.motor.part), self.context['stdout'])
        term.output("%(BOLD)%(WHITE)Firmware: %(NORMAL){0}".format(
            self.motor.firmware), self.context['stdout'])

    def do_profile(self, info):
        """
        Configures the profile for a motor

        profile get accel inch
        profile set accel 490 revs
        """
        parts = info.split()
        getset = parts.pop(0)
        if getset not in ('get','set'):
            return self.error("Incorrect operation", "See 'help profile'")

        component = parts.pop(0)
        if component not in ('run_current',):
            return self.error("Incorrect setting", "See 'help profile'")

        val = parts.pop(0)
        try:
            val = int(val)
        except ValueError:
            return self.error("Invalid value", "Integer required")

        units = None
        if len(parts):
            for id, name in all_units.items():
                if name == parts[-1]:
                    units = id
                    break
            else:
                self.error("{0}: Unsupported units".format(what))

            parts.pop(0)

        if len(parts):
            return SyntaxError("See 'help profile'")

        self.motor.profile.run_current_set(val)
        self.motor.profile.commit()
