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

class NameCommands(Mixin):
    def do_name(self, arg):
        """
        Usage: name <port>

        Name motors connected to the given serial port. The motors are
        assumed to use the mdrive driver.
        """
        port, speed = arg.partition('@')[::2]
        if port == "":
            raise SyntaxError("Serial port must be specified")
        elif not os.path.exists(port):
            raise IOError("Serial port specified does not exist")

        NamingContext(port=arg, context=self.context.copy()).run()

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

    def __init__(self, port, context):
        Shell.__init__(self, context)
        self.context['port'] = port

        self.do_connect("mdrive://{0}".format(port))
        self.motor = self.context['motors']['!'].motor

    def default(self, line):
        match = re.match(r'(?P<name>\w):(?P<sn>\d+)', line)
        if match is None:
            self.error("Naming format is <addr>:<serial number>",
                "see 'help naming' for more information")
        else:
            name = match.groupdict()['name']
            sn = match.groupdict()['sn']
            try:
                self.status("Setting %s to %s" % (sn, name.lower()))
                self.motor.name_set(name.lower(), sn)
                self.status("Motor successfully named")
                if self.confirm(
                    "Would you line to set the baudrate to 115.2k?",
                    default=True):
                    self.status("Switching to 115200 baud")
                    named_motor = MdriveMotor("mdrive://{0}:{1}"
                        .format(self['port'], name.lower()))
                    named_motor.baudrate = 115200
            except AttributeError:
                self.error("Motor does not support naming")
            except Exception as e:
                self.error("{0}: Unable to name motor".format(e.message))

    def help_naming(self):
        print(self.intro)
