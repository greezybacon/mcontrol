from . import Mixin
from .motor import MotorContext

import mcontrol

class MotorConnect(Mixin):
    """
    Facilitates connections to the mcontrol motors
    """

    def do_connect(self, string):
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
        """
        self.status("%(BOLD)%(GREEN)Connecting to {0}%(NORMAL)".format(
            string))
        try:
            if string.startswith('mdrive'):
                motor = mcontrol.MdriveMotor(string)
            else:
                motor = mcontrol.Motor(string)
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
