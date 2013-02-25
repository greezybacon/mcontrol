from mcontrol import Trace

from . import Mixin, initializer

import os.path

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
        trace tee <file>|off [only]

        Trace Levels:
        crash, error, warn, info, debug, data, any
        """
        words = line.split()
        if len(words) < 1:
            return self.error("Incorrect usage", "See 'help trace'")

        subcommand = words.pop(0)
        if not hasattr(self, 'trace_'+subcommand):
            return self.error("Unsupported trace option", "See 'help trace'")

        return getattr(self, 'trace_'+subcommand)(words)

    def trace_add(self, line):
        for channel in line:
            self['trace'].add(channel)

    def complete_trace(self, text, line, start, end):
        if ' add ' in line:
            return [x for x in self['trace'].enum() if x.startswith(text)]

    def trace_list(self, line):
        self.init_trace()
        for channel in self['trace'].enum():
            self.out(channel)

    def trace_tee(self, line):
        """
        Tee trace to a file. Use 'off' to cancel trace to file, 'only' to trace
        to tee file only
        """
        if 'only' in line:
            line.remove('only')
            self['trace-local'] = False
        filename = ' '.join(line)
        if filename.lower() == 'off':
            self['trace-tee'].close()
            self['trace-tee'] = False
            self['trace-local'] = True
        elif not os.path.exists(os.path.dirname(filename)):
            self.error("Folder for tee file does not exist")
        else:
            self['trace-tee'] = open(filename, 'wt')

    @initializer
    def init_trace(self):
        self['trace'] = TeeableTrace(level=50)
        # XXX: HACK
        self['trace'].shell = self
        self['trace-local'] = True
        self['trace-tee'] = False

class TeeableTrace(Trace):
    def __del__(self):
        super(TeeableTrace, self).__del__()
        if self['trace-tee']:
            self['trace-tee'].close()

    def __getitem__(self, name):
        return self.shell[name]

    def emit(self, level, channel, text):
        if self['trace-tee']:
            self['trace-tee'].write("{0}: {1}\n".format(
                self.channel_names[channel], text))
        if self['trace-local']:
            self.shell.status('%(WHITE){0}: {1}%(NORMAL)'.format(
                self.channel_names[channel], text))
