Tracing
=======
Tracing is used instead of logging inside the mcontrol system. This makes
all messages available all the time, regardless of the current log
configuration. The concept is that the daemon can subscribe to tracing to
some level and pipe the traces out to a log file; however, all the trace
data is still available to other connected clients all the time.

Tracing is organized in a publish/subscribe model. Publishers will advertise
trace channels and will write trace messages to those channels with some
trace level attached. To receive trace messages, one simply needs to
subscribe the a trace channel at a particular level.

trace add
---------
    trace add <channel> [level]

Subscribe to the named trace channel at the given level. Advertised trace
channels are available via the `trace list` command.

trace list
----------
    trace list

Displays all currently advertised trace channels. Use `trace add` to
subscribe to messages written to the trace channel.

trace tee
---------
    trace tee <<filename> [only]|off>

Configures an alternate location for the trace message output. There are
basically three uses. By default, trace messages are written to the screen.
This command can add the following options: (1) Write a copy of the trace to
the filename given, (2) (with the `only` flag) only write traces to the
named file, and (3) disable trace writing to the configure file. The usages
for the three options look like

    trace tee trace.out
    trace tee trace.out only
    trace tee off

Trace teeing is only supported globally and only to one file at a time. If
`trace tee` is called twice naming two different files, only the file named
in the second call will receive the trace messages.
