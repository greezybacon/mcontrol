# coding: utf-8
"""
Naming context and commands

Allows for motors in a sub-assembly to be named in manufacturing as well as
in the field when motors are replaced. As intended, the motors are
anticipated to be in factory default mode (9600 baud, no address, and no
party mode).
"""

from __future__ import print_function

from mcontrol import MdriveMotor

from . import Mixin, trim

from cli import Shell
from lib import term

import os.path
import re
import shlex

class NameCommands(Mixin):
    def do_name(self, arg):
        """
        Name motors connected to the given serial port. The motors are
        assumed to use the mdrive driver.

        Usage: name <count> <port>

        Example:
        Name 4 motors on port ttyM2

        name /dev/ttyM2 [4]
        """
        parts = arg.split()
        if len(parts) < 1:
            return self.error("Incorrect usage", "See 'help name'")

        port = parts.pop(0)
        if port == "":
            raise SyntaxError("Serial port must be specified")
        elif not os.path.exists(port):
            raise IOError("Serial port specified does not exist")

        count = False
        try:
            if len(parts):
                count = int(parts.pop(0))
        except ValueError:
            return self.error("Include the number of motors to name",
                "See 'help name'")

        context = NamingContext(port=port, context=self.context.copy(),
            count=count)
        context.cmdqueue = self.cmdqueue
        context.run()

    def complete_name(self, text, line, start, end):
        if line.startswith("/dev/tty"):
            return glob.glob("/dev/tty[SM]*")
        else:
            return ['/dev/tty']

class NamingContext(Shell):

    intro = trim("""
    +====]> Motor naming
    +-------------------------------------------------
    Enter naming instructions in the format of

    <dn>:<sn>

    For example, x:123456789, where <dn> is the name of the device to be
    named, and <sn> is its serial number
    """)

    def __init__(self, port, context, count=False):
        Shell.__init__(self, context)
        self.context['port'] = port
        self.context['named'] = []
        self.context['forced'] = {'baudrate': False, 'firmware': False}
        self.context['ctrl-c-abort'] = True

        self.do_connect("mdrive://{0} recover".format(port))
        self.motor = self.context['motors']['!'].motor
        self.count = count

    def default(self, line, force=False):
        match = re.match(r'(?P<name>\w):(?P<sn>\d+)', line)
        if match is None:
            self.error("Naming format is <addr>:<serial number>",
                "see 'help naming' for more information")
        else:
            name = match.groupdict()['name']
            sn = match.groupdict()['sn']
            for named, snd, speed in self.context['named']:
                if (named, snd) == (name, sn) and not force:
                    return self.status(">>> Skipping already-named motor")
            try:
                self.status(">>> Setting %s to %s" % (sn, name.lower()))
                self.motor.name_set(name.lower(), sn)
                self.status(">>> {0}: Motor successfully named"
                    .format(name.lower()))

                named_motor = MdriveMotor("mdrive://{0}:{1}"
                    .format(self['port'], name.lower()))

                # Force baudrate
                speed = 9600
                if self.context['forced']['baudrate']:
                    rate = self.context['forced']['baudrate']
                    self.status("Switching to {0} baud".format(rate))
                    speed = named_motor.baudrate = self.context['forced']['baudrate']
                elif self.confirm(
                        "Would you line to set the baudrate to 115.2k?",
                        default=True):
                    self.status("Switching to 115200 baud")
                    speed = named_motor.baudrate = 115200

                self.context['named'].append((name, sn, speed))
                if self.count and len(self.context['named']) == self.count:
                    return not force
            except AttributeError:
                self.error("Motor does not support naming")
            except Exception as e:
                self.error("{0!r}: Unable to name motor".format(e))

    def do_force(self, line):
        """
        Force settings on newly-named motors

        Usage:
        force baudrate 115200

        Automatically switch motors to 115.2k baud after successfully naming
        the motors

        Usage:
        force firmware <version> <image filename>
        force firmware 910.000 /path/to/firmware/image.IMS

        Automatically flash and rename the motor if the firmware on the
        motor does not match the version given
        """
        parts = shlex.split(line)

        if len(parts) == 0:
            return self.error("Specify option to force", "See 'help force'")

        option = parts.pop(0)

        if len(parts) == 0:
            return self.error("Specify value to be forced",
                "See 'help force'")

        if option not in ('firmware', 'baudrate'):
            return self.error("Unsupported force option", "See 'help force'")

        elif option == 'baudrate':
            try:
                parts = int(parts[0])
            except ValueError:
                return self.error("Provide a number for the forced baudrate")

        elif option == 'firmware':
            if len(parts) != 2:
                return self.error("Incorrect usage", "See 'help force'")
            elif not os.path.exists(parts[1]):
                return self.error("Firmware image not found")

        self.status("Forcing {0} to {1}".format(option, parts))
        self.context['forced'][option] = parts

    def do_clean(self, line):
        """
        Factory default all motors connected at the specified baudrate. Do
        note that this operation will not be confirmed. Ensure you are
        connected to the correct serial port before embarking on this
        mission.
        """
        parts = line.split()
        if len(parts) == 0:
            return self.error("Provide a baudrate", "See 'help clean'")
        try:
            baudrate = int(parts[0])
        except:
            return self.error("Provide a number for the baudrate", 
                "See 'help clean'")

        print("Cleaning named motors at {0}".format(baudrate))
        motors = MdriveMotor('mdrive://{0}@{1}:*'.format(
            self.context['port'], baudrate), recovery=True)
        motors.factory_default()

        print("Cleaning unnamed motors at {0}".format(baudrate))
        motors = MdriveMotor('mdrive://{0}@{1}'.format(
            self.context['port'], baudrate), recovery=True)
        motors.factory_default()

    def postloop(self):
        """
        Run after all the motors are named to enforce firmware version, if
        set with the 'force' command
        """
        if not self.context['forced']['firmware']:
            return

        version, path = self.context['forced']['firmware']

        for name, sn, speed in self.context['named']:
            named_motor = MdriveMotor("mdrive://{0}@{1}:{2}"
                .format(self['port'], speed, name.lower()))

            if named_motor.firmware != version:
                try:
                    print("{0}: Flashing firmware from {1}"
                        .format(name, path))
                    named_motor.load_firmware(path)
                    # Rename the motor
                    if self.default("{0}:{1}".format(name, sn), force=True):
                        break
                except:
                    self.error("Unable to flash firmware")
                    raise

    def help_naming(self):
        print(self.intro)
