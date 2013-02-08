from . import Mixin
from .motor import MotorContext

import mcontrol

class MotorConnect(Mixin):
    """
    Facilitates connections to the mcontrol motors
    """

    def do_connect(self, line):
        """
        Connect to a motor identified by the connection string. Connection
        strings should be in the format of

        mdrive://<port><@speed>:<address>

        Where <port> is the serial port connecting the motor (/dev/ttyM0 for
        instance), <@speed> is the device speed other than the default of
        9600 baud (@115200 for instance), and <address> is the address of
        the motor to connect to. '!' is the default for unnamed motors, and
        is not necessary. For example

        mdrive:///dev/ttyM0@115200:x

        Will connect to the /dev/ttyM0 port at 115200 baud and attempt to
        talk to a motor named 'x'

        For sticky situations, the motor can be connected to in recovery
        mode where it isn't responding properly to be connected to otherwise
        -- if it's stuck in firmware upgrade mode, for instance. Use the
        "recover" option

        Usage: connect mdrive:///dev/port0[@speed][:name] [recover]

        """
        # Allow shortcuts to be defined in the settings file
        shortcuts = self.get_settings('motors')
        if line in shortcuts:
            line = shortcuts[line]
        parts = line.split()
        recovery = False
        if 'recover' in parts:
            recovery = True
            parts.remove('recover')

        string = parts[0]
        self.status("%(BOLD)%(GREEN)Connecting to {0}%(NORMAL)".format(
            string))
        try:
            if string.startswith('mdrive'):
                motor = mcontrol.MdriveMotor(string, recovery)
            else:
                motor = mcontrol.Motor(string, recovery)
        except mcontrol.NoDaemonException:
            self.error("Daemon is not accessible", "is one running?")
        except ValueError:
            self.error("Bad connection string", "see 'help connect'")
        except mcontrol.CommFailure:
            self.warn("Motor is not responding")
        else:
            # TODO: Retrieve a (unique) name for the motor
            if string.startswith('mdrive'):
                if string[-2] == ':':
                    address = string[-1]
                else:
                    address = '!'

            self.context['motors'][address] = MotorContext(self,
                motor, name=address)

    def complete_connect(self, text, line, start, end):
        """
        Allow completion by the 'motors/prefix' setting or shortcuts defined
        in the settings file
        """
        shortcuts = self.get_settings('motors')
        prefix = shortcuts.pop('prefix', '')
        if text in shortcuts:
            if shortcuts[text] not in line:
                return [shortcuts[text]]
        return [x for x in [prefix] + shortcuts.keys()
            if x.startswith(text)]

    def do_switch(self, name):
        """
        Switch to the context of a different motor. This command will
        completely abandon the current context and switch to the context
        given by <name>
        """
        if name not in self.context['motors']:
            self.error("{0}: Motor not yet connected".format(name))
        else:
            self.afterlife = self.context['motors'][name]
            return True
