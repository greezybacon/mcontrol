from cli import Shell

from mcontrol import all_units, Profile, Event
from mcontrol import NoDaemonException

from lib import term

import math
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
        parts = what.split()
        try:
            position, units = self.get_value_and_units(*parts)
        except ValueError as ex:
            return self.error(repr(ex))
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
        if self.last_move_event and self.last_move_event.data:
            self.out(self.last_move_event.data.stalled)
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
        if units in abbreviations:
            units = abbreviations[units]
        elif type(units) is not int:
            if units not in all_units:
                raise ValueError("Unknown units given")
            units = all_units[units]
        if units in conversions:
            conv = conversions[units]
            value = float(value) * conv[0]
            units = conv[1]
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
        self.last_move_event = self.motor.on(Event.EV_MOTION)
        if wait:
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
        if self.last_move_event and self.last_move_event.isset:
            return
        elif self.last_move_event:
            self.last_move_event.wait()

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
        Stop the motor immediately by slewing down to 0
        """
        self.motor.stop()

    def do_halt(self, line):
        """
        Stop the motor immediately and abort any running microcode routine
        """
        self.motor.halt()


    def do_home(self, line):
        """
        Home the motor using the described method and direction. For
        instance,

        home stop left [1 inch]

        Will home the motor to a hard stop in the left hand (negative)
        direction. If homing to hard stop, the position counter will not be
        reset after the device is homed. In that case, follow the command
        with a 'set position 0' or whatever position is reflected by the
        unit at the location of the hard stop.

        If initial creep velocity is not specified, a default value of 0.5
        inches per second will be assumed.
        """
        parts = line.split()

        operation = parts.pop(0)

        reverse = 'left' in parts
        if 'left' in parts:
            parts.remove('left')
        elif 'right' in parts:
            parts.remove('right')

        if len(parts) < 1:
            value, units = 1000, all_units['mil']
        else:
            value, units = self.get_value_and_units(*parts)

        if operation == 'stop':
            # Software implementation of home to hard stop
            return self._do_home_hard_stop(reverse=reverse,
                rate=(value, units))

    def _do_home_hard_stop(self, rate, reverse=False):
        try:
            self.motor.encoder = 1
        except:
            raise

        # Configure motor profile for first pass homing
        slip = self.motor.profile.slip
        rc = self.motor.profile.run_current

        def find_hard_stop(rate, units):
            event = None
            # Somehow, MDrive motors will require a non-zero move command to
            # detect, signal, and clear the stall flag
            self.motor.slew(math.copysign(2, rate), all_units['mil'])
            self.motor.slew(0)
            self.motor.stalled = False
            while True:
                if not self.motor.moving:
                    if event and event.isset:
                        if event.data.stalled:
                            return self.motor.position
                        # Not a stall event, wait for a stall event
                        else:
                            event.reset()
                    else:
                        self.motor.slew(rate, units)
                        event = self.motor.on(Event.EV_MOTION)
                time.sleep(0.2)

        # Find hard stop at fast speed
        rate, units = rate
        if reverse:
            rate *= -1
        backup = rate
        pos = []
        while True:
            self.motor.profile.run_current = 80
            self.motor.profile.slip = (5000, 'micro-rev')
            self.motor.profile.commit()
            pos.append(find_hard_stop(rate*2, units))

            if len(pos) > 1 and abs(pos[-1] - pos[-2]) < 0.01:
                break

            # Back up for 0.5 seconds
            self.motor.profile.run_current = rc
            self.motor.profile.slip = slip
            self.motor.profile.commit()

            self.motor.move(-backup, units)
            self.motor.on(Event.EV_MOTION).wait()

            rate *= 0.8

        # Reset motor profile
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
                if 'current' in component:
                    val = int(parts[0])
                else:
                    val, units = self.get_value_and_units(*parts)
            except ValueError as ex:
                return self.error("Invalid value", repr(ex))

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

    def do_port(self, line):
        """
        Read, write, or configure an IO port on the device.

        port 1 read
        port 1 set high

        port 1 configure active-high source home
        """
        parts = line.split()

        if len(parts) < 2:
            return self.error("Invalid usage", "See 'help port'")

        try:
            port = int(parts.pop(0))
        except ValueError:
            return self.error("Invalid port number", "Use ports 1-5")

        command = parts.pop(0)

        if command == 'read':
            self.out(self.motor.read_port(port))
        elif command == 'set':
            if len(parts) < 1:
                return self.error("New state required", "See 'help port'")
            value = 1 if parts[0] in ('high',) else 0
            self.motor.write_port(port, value)
        elif command == 'configure':
            args = {}
            # Type
            if 'home' in parts:
                args['type'] = "home"
            elif 'input' in parts:
                args['type'] = "input"
            elif 'output' in parts:
                args['type'] = "output"
            elif 'pause' in parts:
                args['type'] = "pause"
            elif 'moving' in parts:
                args['type'] = "moving"

            if 'active-high' in parts:
                args['active_high'] = True
            elif 'active-low' in parts:
                args['active_high'] = False

            if 'source' in parts:
                args['source'] = True
            elif 'sink' in parts:
                args['source'] = False

            self.motor.configure_port(port, **args)

    def help_units(self):
        return trim("""
        Currently supported units are
        {0}
        """.format(
                ', '.join([x for x in all_units if type(x) is str])
        ))
