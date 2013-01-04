from cli import Shell

from mcontrol import all_units, Profile, Event
from mcontrol import NoDaemonException

from lib import term

import time

abbreviations = {
    'in':   all_units['inch']
}

conversions = {
    all_units['inch']:      (1000, all_units['mil'])
}

class MotorContext(Shell):

    intro = None

    def __init__(self, parent, motor, name=""):
        Shell.__init__(self, context=parent.context)
        self.motor = motor
        self.last_move_event = None
        self.name = name
        self.prompt_text = Shell.prompt_text[:-2] + ":{0}> ".format(name)
        self.parent = parent
        self.context['profiles'] = {}

    def do_get(self, what):
        try:
            if hasattr(self, '_do_get_' + what):
                return getattr(self, '_do_get_' + what)()
            else:
                self.out(getattr(self.motor, what))
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

    def _do_set_position(self, what):
        units = None
        if ' ' in what:
            position, units = what.split(' ',1)
        else:
            position = what
        position = int(position)
        self.motor.position = position, units

    def _do_get_units(self):
        self.status("Motor is configured with units of: "
            + all_units[self.motor.units])

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
        if type(units) is not int:
            if units not in all_units:
                return self.error("Unsupported units", "See 'help units'")
            units = all_units[units]

        if len(parts):
            return usage()

        self.motor.scale = int(scale), units

    def _do_get_stalled(self):
        if self.last_move_event and self.last_move_event.data.stalled:
            self.out(True)
        else:
            self.out(self.motor.stalled)

    def get_value_and_units(self, value, units=None):
        """
        Allows the user to input units with a command, and the system will
        convert them to the unit numbers used by mcontrol. Also, if a
        floating point number is given, the value will be converted to a
        smaller unit automagically
        """
        units = units or self.motor.units
        if type(units) is not int:
            if units not in all_units:
                raise ValueError("Unknown units given")
            units = all_units[units]
        if "." in value:
            if units in conversions:
                conv = conversions[units]
                value = float(value) * conv[0]
                units = conv[1]
            else:
                raise ValueError("Integer value required for these units")
        else:
            value = int(value)

        return value, units


    def do_move(self, where):
        self.last_move_event = None
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

        if len(parts) > 2:
            return self.error("Incorrect usage", "see 'help move'")

        try:
            value, units = self.get_value_and_units(*parts)
        except ValueError as ex:
            return self.error(repr(ex))

        func(value, units)
        if wait:
            self.last_move_event = self.motor.on(Event.EV_MOTION)
            self.last_move_event.wait()

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

    def do_wait(self, line):
        """
        Wait indefinitely until current motion is completed
        """
        if not self.motor.moving:
            return
        self.motor.on(Event.EV_MOTION).wait()

    def do_slew(self, line):
        """
        Slew the motor at the given rate. Units if not specified are assumed
        to be in the scale units per second

        Example: slew 200 mil
        Slew motor at 200 mils per second
        """
        parts = line.split()

        if len(parts) > 2:
            return self.error("Incorrect usage", "see 'help slew'")

        try:
            value, units = self.get_value_and_units(*parts)
        except ValueError as ex:
            return self.error(repr(ex))

        self.motor.slew(value, units)

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
        slip = self.motor.profile.slip
        units, scale = self.motor.units, self.motor.scale
        self.motor.scale = 1000, 'milli-rev'
        rc, self.motor.profile.run_current = self.motor.profile.run_current, 35
        self.motor.profile.slip = (3, 'milli-rev')
        self.motor.profile.commit()

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

        self.motor.units = units
        self.motor.scale = scale
        self.motor.profile.slip = slip
        self.motor.profile.run_current = rc
        self.motor.profile.commit()

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

        Profiles can also be stashed and later restored using the 'save' and
        'use' subcommands.

        profile stash <name>
        profile use <name>
        """
        parts = info.split()
        getset = parts.pop(0)
        if getset not in ('get','set','use','stash'):
            return self.error("Incorrect operation", "See 'help profile'")

        component = parts.pop(0)

        if getset == "set":
            try:
                val, units = self.get_value_and_units(*parts)
            except ValueError:
                return self.error("Invalid value", "Integer required")

        elif getset == "stash":
            self.context['profiles'][component] = self.motor.profile.copy()
            return

        elif getset == "use":
            if component not in self.context['profiles']:
                return self.error("{0}: Profile not yet created",
                    "Use 'profile save'")
            self.motor.profile = self.context['profiles'][component]
            return

        try:
            if getset == "set":
                if 'current' in component:
                    setattr(self.motor.profile, component, val)
                else:
                    setattr(self.motor.profile, component, (val, units))
                self.motor.profile.commit()
            elif getset == "get":
                val = getattr(self.motor.profile, component)
                self.info("{0} {1}".format(val, self.motor.units))
            else:
                return self.error("{0}: Unsupported profile command"
                    .format(getset))
        except AttributeError:
            self.error("{0}: Unsupported profile component"
                .format(component))

    def help_units(self):
        return trim("""
        Currently supported units are
        {0}
        """.format(
                ', '.join([x for x in all_units if type(x) is str])
        ))
