from cli import Shell

from mcontrol import all_units, Profile
from mcontrol import NoDaemonException

class MotorContext(Shell):

    intro = None

    def __init__(self, parent, motor, name=""):
        super(MotorContext, self).__init__(context=parent.context)
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

        scale 0.7 per inch
        scale 0.7 inch per rev

        Means 0.7 revolutions per inch of travel, or 0.7 inches of travel
        per revolution
        """
        scale, *per, units = string.split(' ', 2)
        if len(per) == 1:
            per = per[0]
        else:
            units, per = per
            scale = 1 / float(scale)
        if per not in ("per", "/"):
            self.error("Incorrect usage", "see 'help set scale'")
            return

        self._do_set_units(units)
        self.motor.scale = float(scale) * 1e6

    def _do_set_name(self, name):
        if ':' not in name:
            self.error("Naming format is <addr>:<serial number>")
            return

        name, sn = name.split(':')
        try:
            print("Setting %s to %s" % (sn, name.lower()))
            self.motor.name_set(name.lower(), sn)
        except AttributeError:
            self.error("Motor does not support naming")
        except Exception as e:
            self.error("{0}: Unable to name motor".format(e.message))

    def do_move(self, where):
        func = self.motor.move
        units = None

        parts = where.split()
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
                return
            except:
                raise
        self.error("Incorrect usage", "see 'help move'")

    def do_profile(self, info):
        """
        Configures the profile for a motor

        profile get accel inch
        profile set accel 490 revs
        """
        op, setting, units = info.split()

