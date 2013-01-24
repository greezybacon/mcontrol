from mcontrol import Trace

from . import Mixin, initializer

class TraceCommands(Mixin):

    def do_trace(self, line):
        """
        Enable, disable, and configure tracing of the mcontrol library and
        attached drivers.

        Subcommands:
        list            List trace channels available for tracing
        show            Show current trace configuration
        add             Add/change channel subscription
        remove          Remove channel subscription
        tee             Write trace output to a file

        Usage:
        trace list
        trace show
        trace add <channel> [level]
        trace remove <channel>
        trace tee <file>|off
        """
        words = line.split()
        if len(words) < 1:
            return self.error("Incorrect usage", "See 'help trace'")

        subcommand = words.pop(0)
        if not hasattr(self, 'trace_'+subcommand):
            return self.error("Unsupported trace option", "See 'help trace'")

        return getattr(self, 'trace_'+subcommand)(words)

    def trace_add(self, line):
        self.init_trace()
        for channel in line:
            self['trace'].add(channel)

    def trace_list(self, line):
        self.init_trace()
        for channel in self['trace'].enum():
            self.out(channel)

    def init_trace(self):
        if 'trace' not in self:
            self['trace'] = Trace(level=50)
