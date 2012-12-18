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
            self.error('{0}: File does not exist'.format(what))
        else:
            what = os.path.abspath(what)
        self.motor.load_microcode(what)

    def complete_load_microcode(self, text, line, start, end):
        if not text:
            return glob.glob("*")
        else:
            return glob.glob(text + "*")

