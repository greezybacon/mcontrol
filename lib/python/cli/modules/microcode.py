from . import Mixin

import glob
import os.path

class MicrocodeMixin(Mixin):

    def do_install(self, what):
        """
        Usage: install <motor.mxt>

        Loads microcode from the given file. Any microcode currently
        installed in the motor will be removed before the new microcode is
        inserted.
        """
        if not os.path.exists(what):
            # Apply default path from settings file
            installed = self['settings']['paths:microcode'] + '/' + what
            if not os.path.exists(installed):
                return self.error('{0}: File does not exist'.format(what))
            what = installed
        what = os.path.abspath(what)
        self.motor.load_microcode(what)

    def complete_load_microcode(self, text, line, start, end):
        if not text:
            return glob.glob("*")
        else:
            return glob.glob(text + "*")

    def do_flash(self, what):
        """
        Usage: flash <firmware.IMS>

        Loads the firmware image onto the motor. This will also imply a
        factory reset on the motor and will clear any microcode and settings
        previously saved in the motor.
        """
        if not os.path.exists(what):
            # Apply default path from settings file
            installed = self['settings']['paths:firmware'] + '/' + what
            if not os.path.exists(installed):
                return self.error('{0}: File does not exist'.format(what))
            what = installed
        what = os.path.abspath(what)
        self.motor.load_firmware(what)
