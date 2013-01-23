from mcontrol import Library

from . import Mixin

class StandaloneCommands(Mixin):

    def do_standalone(self, line):
        """
        Runs the command-line interface in standalone mode. That is, without
        a separate daemon process to interface with the motors. This mode
        assumes that no other running process (minicom, daemon, or another
        cli) is using any of the ports and/or motors that will be in use
        while standalone mode is enabled

        Usage:

        standalone [disable] [reconnect]

        Note: If changing between standalone mode and daemon mode, all
        motors will need to be reconnected. The 'reconnect' switch can be
        used to automatically reconnect all the motors.
        """
        words = line.split()
        if 'disable' in words:
            Library.run_out_of_process()
        else:
            Library.run_in_process()
            import os
            # Load all the motor drivers -- this would be nice to make a
            # separate cli command. XXX: Adjust this to the final resting
            # place of the drivers
            Library.load_driver(os.path.dirname(__file__)
                + "/../../../../drivers/mdrive.so")

