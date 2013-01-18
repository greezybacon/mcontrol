from __future__ import print_function

from modules import trim
from lib import term

import cmd
import inspect
import re
import shlex
import sys

class Shell(cmd.Cmd, object):

    prompt_text = "mcontrol> "
    intro = """
               ,       .        .
._ _  _. _ ._ -+-._. _ | ___  _.|*
[ | )(_.(_)[ ) | [  (_)|     (_.||

Copyright (c) 2012 Manchac Technologies, LLC
Version 0.1-beta
"""[1:]

    afterlife = None
    context = {
        'env': {},
        'motor': None,
        'motors': {},
        'stdout': sys.stdout,
        'stderr': sys.stderr,
        'quiet': not sys.stdin.isatty(),
        'tests': {},
        'ctrl-c-abort': False,
    }

    def __init__(self, context=None, script=None):
        cmd.Cmd.__init__(self)
        if context and type(context) is dict:
            self.context.update(context)
        if script:
            self.cmdqueue = open(script, 'rt').readlines()

    def __getitem__(self, what):
        return self.context[what]

    def __setitem__(self, what, item):
        self.context[what] = item

    @property
    def prompt(self):
        if not self.context['quiet']:
            return self.prompt_text
        else:
            return ''

    def confirm(self, prompt, default=False):
        choices = "Y|n" if default else "y|N"
        while True:
            answer = input(prompt + " [{0}] ".format(choices))
            if answer == '':
                return default
            elif answer.lower() in ('y','n','yes','no'):
                return answer.lower()[0] == 'y'

    def do_EOF(self, line):
        return self.do_exit(line)

    def do_exit(self, line):
        return True

    def emptyline(self):
        """Do nothing on empty line"""
        pass

    @classmethod
    def mixin(cls, mixin):
        for prop in dir(mixin):
            method = getattr(mixin, prop)
            if inspect.ismethod(method):
                # Python2
                setattr(cls, prop, method.im_func)
            elif inspect.isfunction(method):
                # Python3
                setattr(cls, prop, method)
                method.__doc__ = trim(method.__doc__)

    def out(self, what):
        where = self.context['stdout']
        if type(where) is file:
            if type(what) not in (str, unicode):
                what = repr(what)
            where.write(what + '\n')
        else:
            # Internal pipes like what's used in the testing module
            where.write(what)

    def status(self, what):
        term.output(what, self.context['stderr'])

    def error(self, what, hint=""):
        term.output("%(BOLD)%(RED)*** {0}%(NORMAL){1}".format(
            what, ': ' + hint if hint else ''),
            self.context['stderr'])

    def warn(self, what, hint=""):
        term.output("%(BOLD)%(YELLOW)!!! {0}%(NORMAL){1}".format(
            what, ': ' + hint if hint else ''),
            self.context['stderr'])

    def precmd(self, line):
        # Drop comments
        line = re.sub(r'#.*$', '', line)

        # Substitute {var} expressions
        return line.format(**self['env'])

    def onecmd(self, str):
        try:
            return cmd.Cmd.onecmd(self, str)
        except KeyboardInterrupt:
            if not self['ctrl-c-abort']:
                return False
            raise
        except Exception as e:
            self.error("{0}: (Unhandled) {1}".format(
                type(e).__name__, e))
            return True

    def run(self):
        shell = self
        while True:
            shell.cmdloop()
            if shell.afterlife:
                shell = shell.afterlife
                shell.afterlife = None
            else:
                self.halt_all_motors()
                break

    def halt_all_motors(self):
        for name, context in self['motors'].items():
            try:
                context.motor.stop()
            except:
                pass

# Warning: Major magic mojo
#
# Import modules from the module directory and finds all the subclasses of
# the Mixin class. Of those, the Shell.mixin() method is called to add the
# module pieces into the cli environment
import modules
from types import ModuleType

# More on dynamic importing here
# http://docs.python.org/2/library/functions.html#__import__
mods = __import__('modules', globals(), locals(),
        modules.__all__, -1)

for name in modules.__all__:
    mod = getattr(mods, name)
    for name, cls in vars(mod).items():
        if type(cls) is type and issubclass(cls, modules.Mixin):
            Shell.mixin(cls)

if __name__ == '__main__':
    if len(sys.argv) > 1:
        Shell(script=sys.arv[1]).run()
    else:
        Shell().run()
