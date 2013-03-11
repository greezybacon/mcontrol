MControl Concepts
======================

Daemon
======
MControl ships with a daemon process which can be used to allow multiple
client programs to attach to several motors. The daemon will perform several
common tasks:

  * cache the state of the motors to allow for repeat connections to motors
    to return without communication delays
  * subscribe to tracing on a configurable level and log messages out to a
    file. Custom tracing by level or channel can be performed by an attached
    client, but configured tracing will be logged automatically.
  * thread and queue motor requests so motor tasks can be performed in
    parallel if not blocked by IO
  * support high-priority motor tasks (such as emergency-stop).
    High-priority tasks are automatically placed ahead of any tasks
    currently in the motor work queue

Clients
=======
MControl needs to support two types of clients: standalone and daemon. In
standalone mode, the client process will load the driver for the motors to
be attached and will communicate to the drivers in-process. In daemon mode,
the daemon process will load the driver for the motors and only the daemon
will communicate with the motors. The daemon configuration allows several
different processes to communicate with the same motor.

When clients attach to the mcontrol library, they default to operating
cross-process, that is, in daemon mode. Any calls that a daemon will
normally service are sent cross process via POSIX message queues to the
daemon. If no daemon is running the calls will block indefinitely or
timeout, depending on the type of call.

Call Mode
---------
The call mode can be changed at any time using the `mcClientCallModeSet`,
which can be set to `MC_CALL_IN_PROCESS` or `MC_CALL_CROSS_PROCESS`. Again,
cross process is the default and will use a separate daemon process to
communicate with the motors. If a client is switching to in-process calling,
it will likely need to load drivers for motor using the `mcDriverLoad`
routine, which will attempt to dynamically load the driver library
requested.

Drivers
=======
MControl drivers will need to adhere to a thin interface which will allow
a consistent command base for each motor to be controlled

Driver Interface
----------------
The driver interface serves as the main entry between the mcontrol library
and the device drivers. In the software, it is referred to a as
`DriverClass`.

The driver can expose information about itself via the `name`,
`description`, and `revision` fields. The `name` field is important, as it
will be used to identify which connection strings the driver will handle.

Driver Registration
-------------------
When the device driver is loaded, it should call `mcDriverRegister` and pass
a pointer to its driver interface (`DriverClass *`). The driver registration
process will register the driver and will pass it connection strings that
declare it as the driver.

The registration should happen **automatically**. Generally, decorate a
simple function inside the driver as a `ctor` which will allow it to run
when the software is either dynamically linked or when the software starts
if the driver gets built into an executable.

Connecting
==========
Motors are accessed through a somewhat generalized connection string. The
connection string syntax uses a URI as its base and takes the form of

    <driver>://<driver-specific-connection>/

*The trailing slash is allowed to be dropped*

MDrive Connection Strings
-------------------------
The MDrive driver registers itself under the name of `mdrive`, so the driver
portion of the URI will be `mdrive`. The driver supports the following
connection options

    mdrive://<port>[@speed][:a]

Where `port` is the serial port to access the motor, `speed` is a valid
baudrate that the MDrive motors support, and `a` is the device name of the
motor. If unspecified, the unnamed motor will be targeted with party-mode
disabled.

Library Middleground
====================
A common mcontrol library is used to attach client programs to the daemon
and to load drivers for standalone operation.

Measurements
------------
Motor commands involving measurements are broken down in the library in
order to separate the concept of distance in each respective motor and motor
driver. For instance, the MDrive motors use *steps* as their increment of
motion. Even in the MDrive world, the concept of a step is skewed based on
the configuration of the motor.

Therefore, the library keeps track of all motion distances in whole
revolutions of a motor. The revolutions are kept in a fixed, six-decimal
form, and are referred to as *micro-revolutions*. From the perspective of
the library, the motor drivers will only accept micro-revs as distance
arguments and will return micro-revs as the result of any response, query,
or answer from the driver.

As a further abstraction, the library supports translating measurements
between several common engineering units. The engineering unit must be set
on a motor before any command is invoked involving a distance measurement
(like a move, for instance).

Scale
-----
The scale of the motor is kept as the main conversion between micro-revs and
the main engineering unit of the motor. The scale represents the number of
units traveled by the motor for one revolution. The scale is to be
configured in a fixed, six-decimal format. As an example, if a motor travels
2.058 inches per revolution, then the scale would be calculated as

    scale = 1e6 / 2.058 = 485909

In other words, the motor needs to rotate 0.485909 revolutions to travel one
inch. To configure a motor to operate on a 2.058 inch lead screw, one would
use

    mcUnitScaleSet(motor, 485909, INCH);

Drivers and Motors
------------------
Drivers are multiplexed in order to allow separate *views* of motors. In
other words, if two programs connect to the same motor, they are allowed to
operate the motor differently.

As motors are connected, a driver instance is created to service the motor.
The driver class is reused to service all driver instances of the same type.
The driver instance is cached under the connection string used to retrieve
it.

If a motor connection is requested and the connection string is in the
driver instance cache, the driver instance will be reused. Therefore,
connections to the same motor by the same connection string will be reused.
Some characteristics of the motor are allowed by be different between motor
connections. These characteristics, such as the scale and major units, are
required to be set after every connection, because they are not shared
across connections.

Cross Process
-------------
Cross process calling uses the proxy model. At build time, for each daemon
function (those declared with `PROXYSTUB`), four functions are created with
various suffixes. For ease of example, we will follow the `mcConnect`
function:

  * **mcConnect**: The function declared without an extension is used as a
    trampoline. The trampoline will select either the actual implementation
    or a proxy function, depending on whether the current call mode is
    in-process or cross-process respectively.
  * **mcConnectImpl**: The function declared in the `.c` file with the
    `PROXYIMPL` define is used as the actual implementation. The `Impl` part
    is added automatically and should not be included in the function name
    in the `.c` file.
  * **mcConnectProxy**: The proxy function is automatically generated by
    `client.c.py` and is executed client side. It is called by the
    trampoline function (`mcConnect` in this case). Its job is to pack up
    the arguments into a structure and pass them cross-process to the `Stub`
    function, as well as to unpack the returned value from the `Stub`
    function and detect calling errors (timeouts, interruptions, etc.) and
    raise them appropriately.
  * **mcConnectStub**: The stub function is automatically generated by
    `client.c.py` and is executed daemon-side. Its job is to unpack the
    arguments received in the queued message and call the `Impl` function.
    The returned value from the `Impl` function is then packed back up into
    the argument structure and returned back to the remote caller. Also, any
    arguments declared with `OUT` are also marshaled back to the caller.

Cross process messages are dispatched using the `mcDispatchProxyMessage`,
which will use an automatically-generated table in `dispatch.h` to lookup
the `Stub` function.

Impl Functions
--------------
Remote functions are declared with the `PROXYIMPL` define

    int
    PROXYIMPL(mcConnect, String * connection, OUT MOTOR * motor,
        bool recovery)

The `PROXYIMPL` define will add one additional, magic parameter, `CONTEXT`.
The context is magic and is a pointer to a `struct call_context`, which will
have information about how the call originated, as well as some basic common
work already performed.

  * `.client_pid` the process-id of the caller. For data structures that
    check the PID of the client along with some other arguments.
  * `.motor` a `Motor *` that points to a daemon-side motor. Since pointers
    cannot be shared cross-process, a motor handle or id number is used to
    represent a motor connection in the remote client. The lookup of that
    motor id to the daemon-side `Motor *` is performed automatically before
    the `Impl` function is invoked. If the motor cannot be found by the id
    number given, the `Impl` function will never be invoked. An error will
    be returned to the caller. The parameter with the `MOTOR` flag is
    automatically used as the input to this lookup.
  * `.inproc` will be `true` if the call was made in-process
  * `.outofproc` will be `true` if the call was made out-of-process

Events
======
**NOTE**: Events operate slightly differently than described currently. They
should/will be revised to follow this concept, though.

Both the common library and drivers can source events. Events are divided
into two categories: signals and notifications. Signals are used to signal
static events, such as a stall or motion-completion. Notifications are used
to signal conditional events, such as the arrival or crossing of a certain
position, speed, etc. Notifications must be reset after every firing;
however, signals will continue to fire as long as they are registered.

Driver Events
-------------
Driver events are subscribed to using the `subscribe`, `notify`,
`unsubscribe` members of the registered driver class. `subscribe` is used
for receiving unconditional events, and `notify` is used to receive
conditional events. Events subscribed with `notify` must be re-subscribed
after every firing.

Trapping Events
---------------
Driver events as well as events sourced in the library are made available to
the client software via the `mcSubscribe` and `mcNotify` methods. For event
notifications, the client will call `mcSubscribe` to setup the registration
and event callback handler, and will pass the event registration id to
`mcNotify`. If separate callback handlers are required, use multiple calls
to `mcSubscribe` to configure the various handlers your software requires.

Waiting for Events
------------------
Events can be used synchronously or asynchronously. After calling
`mcSubscribe` events are configured to work asynchronously. The given
callback will be invoked every time the event is fired. To work with events
synchronously, use the `mcEventWait` routine, which will block until
interrupted by `SIGINT` or until the event has been signaled and the
callback has been invoked.

If `SIGINT` is received, it will be reraised to allow your standard signal
handlers to handle it properly.

Firing Events
-------------
Inside the library, the `mcSignalEvent` routine can be used to signal an
event to registered subscribers. The function will receive the driver
instance to fire the event on, and the event information in the form of a
(`struct event_info *`). The event will be fired on all motors that point to
the given driver that currently subscribe to the named event.

Generally, the `mcSignalEvent` routine is used as the registered endpoint for all
driver-sourced events. This makes the event firing process consistent inside
the library. Ultimately, driver-sourced events are just re-fired from the
library.

Locks
=====
**NOTE**: Locks are not implemented at the moment

Locks allow the movement of a motor to be restricted or prohibited. A lock
is placed on a motor similar to a lien on property. Any motor can place a
lock on any other motor; however, only the lock holder can remove the lock.
When a motor is locked, any calls to a move routine on that motor will
return an error code indicating the motor is currently locked.

Placing Locks
-------------
Locks are placed with the `mcLock` method and removed with the `mcUnlock`
method. The `mcLock` method will return a lock id valid only for the caller,
and the `mcUnlock` will operate on those lock ids. Static locks prohibit
movement of any kind on the motor.

Conditional Locks
-----------------
Motors can allow a motor to move within a safe envelope. The
`mcLockSafeEnvelope` method can be used to restrict the movement of a motor
to the confines of the envelope. The envelope is limited to a single axis,
and the motor must be inside the envelope for the lock to succeed. Until the
lock is removed, the motor will be forbidden to leave the envelope.

If a move is requested of the locked motor that would move it outside the
envelope, the move is refused and the motor is left in its current position.

Checkback Locks
---------------
Another form of the conditional lock is a checkback lock which allows a
callback function to be declared. When executed, the callback will determine
if a move target is allowable for the motor. All the lock callbacks are
invoked inside the `mcMoveIsSafe` method, which is used to determine if a
move request is permissible.

Tracing
=======
Tracing is used instead of logging to allow client programs to view
different levels of trace messages without reconfiguring or restarting any
of the mcontrol software.

Tracing is optimized to format trace messages only once for all subscribers,
and will forgo formatting the message at all if there are no subscribers to
the given channel and level of the traced message.

Channels
--------
Tracing is organized into channels. Channels are created with the
`mcTraceChannelInit` routine which will return a unique channel id. The id
is passes to the `mcTrace` routine and friends.

Subscribers
-----------
Tracing is subscribed with `mcTraceSubscribe` for daemon programs only. For
remote and in-process clients, use `mcSubscribe` to subscribe to the
`EV_TRACE` event, and name a callback function to receive the trace
messages. Then, use the `mcTraceSubscribeRemote` to register for trace
messages at a certain level.

For either method, use `mcTraceSubscribeAdd` and `mcTraceSubscribeRemove` to
adjust the channels registered in the subscription.
