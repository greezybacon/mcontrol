from __future__ import print_function

from modules import trim
from lib import term

import cmd
import inspect
import os.path
import re
import readline
import sys

class Shell(cmd.Cmd, object):

    prompt_text = "mcontrol> "
    intro = """
               ,       .        .
._ _  _. _ ._ -+-._. _ | ___  _.|*
[ | )(_.(_)[ ) | [  (_)|     (_.||

Copyright (c) 2012 Manchac Technologies, LLC
Version 0.1-1
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
    initializers = []
    destructors = []

    def __init__(self, context=None, scripts=None, options=None):
        cmd.Cmd.__init__(self)
        if context and type(context) is dict:
            self.context.update(context)
        if options and options.standalone:
            self.do_standalone('')
        # Only run initializers for very first instanciation
        if type(self) is Shell:
            for x in self.initializers:
                if hasattr(x, 'im_func'):
                    # Python2
                    x.im_func(self)
                else:
                    # Python3
                    x(self)
        if scripts:
            for s in scripts:
                self.cmdqueue = open(s, 'rt').readlines()

    def run_destructors(self):
        if type(self) is Shell:
            for x in self.destructors:
                if hasattr(x, 'im_func'):
                    # Python2
                    x.im_func(self)
                else:
                    # Python3
                    x(self)

    def __getitem__(self, what):
        return self.context[what]

    def __setitem__(self, what, item):
        self.context[what] = item

    def __contains__(self, what):
        return what in self.context

    @property
    def prompt(self):
        if not self.context['quiet']:
            return self.prompt_text
        else:
            return ''

    def confirm(self, prompt, default=False):
        if sys.version_info[:2] < (3,0):
            # Python3 renames raw_input -> input
            input = raw_input

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
            if hasattr(method, 'ignore'):
                continue
            if inspect.ismethod(method):
                # Python2
                setattr(cls, prop, method.im_func)
            elif inspect.isfunction(method):
                # Python3
                setattr(cls, prop, method)
                method.__doc__ = trim(method.__doc__)
            if hasattr(method, 'initializer'):
                cls.initializers.append(method)
            if hasattr(method, 'destructor'):
                cls.destructors.append(method)

    def out(self, what):
        where = self.context['stdout']
        if where.isatty():
            if type(what) is not str:
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

        if line not in ("EOF","") and len(self.cmdqueue) == 0:
            # XXX: not self.cmdqueue will still record the last queued
            #      command
            readline.add_history(line)

        # Expand home folder
        line = os.path.expanduser(line)

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
                break
        self.halt_all_motors()
        self.run_destructors()

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

# Commandline option parsing
import optparse
parser = optparse.OptionParser()
parser.add_option('--standalone',help="Run script in standalone mode",
    action="store_true", dest="standalone", default=False)

if __name__ == '__main__':
    options, args = parser.parse_args()
    Shell(options=options, scripts=args).run()
