from . import Mixin

import glob
import os.path

class MicrocodeMixin(Mixin):

    def do_load_microcode(self, what):
        """
        Usage: load_microcode <motor.mxt>

        Loads microcode from the given file.
        """
        pass

    def complete_load_microcode(self, text, line, start, end):
        if not text:
            return glob.glob("*")
        else:
            return glob.glob(text + "*")

