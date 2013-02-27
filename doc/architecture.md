Organization
============
MControl is organized into a few pieces, centered around a central library.
The central library is utilized to a allow common handling of motors and is
organized so that calls into the library can be made cross-process to a
daemon owning all motor instances as well as in-process assuming that other
running processes might own other, inaccessible motors.

libmcontrol
-----------
So libmcontrol.so is the library that glues the mcontrol architecture
together. In a general sense, it provides only a few pieces:

  - Abstraction from drivers controlling motors
  - Transparent in-process and cross-process calling
  - Common utilities used by drivers

There are two types of functions accessible in the library: native
functions, and proxy/stub functions.

Native functions are mostly used by the library itself and attached drivers.
In general, attached client softwares should not make use of the native
functions. Ultimately, there is no harm is making calls to native functions,
but the results may be unanticipated. Native functions include the utility
pieces such as engineering unit conversion, tracing, and callbacks.

Proxy functions are mostly used by the attached client. Proxy functions will
either make an in-process call or a cross-process call depending on the
value seen in mcClientCallModeGet() function.

If the call mode is configured to be MC_CALL_CROSS_PROCESS (the default),
the call will branch to a proxy function which will pack up the function's
arguments and pass then via a POSIX message queue to the daemon. The daemon
(referred to as "server-side" in the code) will receive the message and make
the call to the appropriate stub function. The stub function will unpack the
arguments and call the actual function implementation (the "real" function
as usually described in the proxy model).

In-process calls will simply branch to the "real" function implementation
without leaving the process.

This model allows common implementation code to be used either in process
(micro-daemon mode) or cross-process (with a traditional daemon). In terms
of the Dosis software suite, this model will ease a transition to this
software without the use of a standalone daemon -- that is, just as a
command-line tool, and will also work well for standalone environments, such
as manufacturing. 

Calling Conventions
-------------------
As described above, there are two calling modes performed in the libmcontrol
library: in-process and cross-process. The mode used is defined with a call
to mcClientCallModeSet(). In process calls will work something like:

    +----------+     +---------------+     +---------------+
    |  client  | --> | function-stub | --> | function impl |
    +----------+     +---------------+     +---------------+

In-process calling is pretty straight forward. The only piece perhaps
unexpected is the stub part, which is used as a trampoline here to the
"real" function implementation.

Out-of-process calls are slightly more complicated and work something like

    +----------+    +---------------+    +----------------+
    |  client  | -> | function stub | -> | function proxy |
    +----------+    +---------------+    +----------------+
                                                  |
                                                  V
         +-------------+    +--------+    +---------------+
         | daemon stub | <- | daemon | <- | message queue |
         +-------------+    +--------+    +---------------+
                |
                V
      +----------------+                
      |  function impl |
      +----------------+

Most of the code to implement this is auto-generated, so don't get too
scared reading through the control flow. Also, the code works without any
third-party libraries or dynamic magic, so good performance is preserved.
According to most sources, a call through a POSIX message queue has about a
31 microseconds round-trip time. So this may look heavy, but it does not add
a significant overhead.

Design defense aside, a cross-process call is proxied through a message
queue to a traditional daemon, which will receive the request, lookup the
stub function in a function table, and used the stub to unpack the arguments
and call the "real" function implementation. The other trick performed in
the cross-process model is the concept of "OUT" parameters. Parameters
declared with the OUT attribute will be marshaled back to the client. Since
the value written by the function implementation is only visible in the
daemon, the daemon stub code will automatically pack up the modified OUT
parameter values in the response message returned to the client.

mcontrol daemon
---------------
The advantage of the traditional daemon mode is that, in a production unit,
all motors are accessible to all processes, which will make attaching locks
to motors more consistent. It will also allow for real-time tracing of the
mcontrol software via a command-line interface. Other neat features apply as
well, such as changing microcode, firmware, profile settings, etc., in
motors without the need to restart the Dosis software. And such can be done
by any connected client, such as a command-line tool, or an automated
software migration tool.

The daemon is slated to provide other features geared more toward a
production unit, such as logging of traced messages to a common log file, as
well as threading access to motor drivers, so concurrent access to motors on
different communication channels is possible without blocking.

Clients
-------
The base software includes two client pieces: A Python library called
'mcontrol', and a command-line interface written in Python. More details on
the cli can be found [in the cli](cli.md) documentation.
