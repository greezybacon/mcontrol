Automated Testing
=================
The test module in the cli allows for the definition of standardized tests.
The command selection is flexible enough to define some pretty complicated
procedures.

Creating and Running
--------------------
Tests are created with the "create" command, which will enter macro
recording mode. "save" is used to exit record mode. "run" is used to execute
the commands in a given test. Refer to detailed descriptions below for more
details.

Structure
---------
All commands fit on exactly one line, and each single line defines exactly
one command.  Therefore, code written in tests can be subdivided by using
labels.  Execution can be moved around by using the "call", "return" and
"goto" statements.

Multiple commands can be placed on the same line if semi-colons are used to
separate them. See the block example below.

Blocks are supported with the "do" and "done" commands. So you can write
code that looks something like

    if condition; do
        print "Something"
    done

Whitespace around the commands is ignored. Feel free to use whitespace to
help in marking code as part of your respective labeled "blocks".

Variable Substitution
---------------------
Variables placed in braces are automatically converted to their current
value before the command is executed. This occurs in the parse phase of the
command, which is executed every time the command is to be executed.
Therefore,

    let prompt = "Hello"
    print "{prompt}, World!"

Will write "Hello, World!" to the output. Replacement happens before the
quoted string literal is evaluated as a Python expression.

Subcommands
-----------
Subcommands can be used with any command. For any command given, parts
placed in brackets are evaluated as a separate command, whose output will
replace the bracket expression. For instance

    print "[print 'Hello'], World!"

Will result in "Hello, World!" written to the output.

Commanding Motors
-----------------
**Warning:** The `switch` command is not supported inside the test
environment. If you use the `switch` command, it will abandon the testing
environment altogether.

Commands which should run in the context of a connected motor can by
indicated by the motor name followed by an arrow. The command after the
arrow will be executed in the context of the motor named. For instance

    name -> move to 5 inch

Would have the same effect as

    switch name
    move to 5 inch

Motor commands are also valid inside bracketed subcommands.

Parallel Tasks
--------------
Testing can be divided into parallel components and controlled via tasks.
Tasks are created from a label in the same test. Take care to ensure that
the same code is not being executed at the same time by placing `succeed`
commands at the end of your parallel labels, or you might end up with
something like soldiers from core wars.

Tasks can be synchronized with the `task send` and `yield` commands. Yield
causes a task to suspended until a corresponding send is made to the task.
The mechanism can be used to exchange data between the tasks as well. Data
can be included with both the `task send` and `yield` commands and both of
them can return values if used in a subcommand. This model follows the
coroutine pattern like the one found in Python generators.

Under the hood, tasks are implemented with standard daemon threads.
Therefore, their execution will be canceled with the main thread that
created them exits. To cause a task or the main thread to suspend until the
completion of a task (by reaching the end of the script or a `succeed` or
`fail` line or such), use the `task join` command. The state of the task is
propagated to the joining task when the join command completes. Therefore,
if a task fails or aborts, it will cause the joining task to fail or abort
as well. The same happens when communicating to a task with the `task send`
command.

Test Commands
=============
These commands are available inside the testing environment too, but not
visa versa. These commands are used to construct and execute a test.

create
------
    create <test-name>

Creates or replaces a test with the given name. After this command, the
environment will be switched into a macro recording mode. Any command
entered will not be executed immediately. Instead, the commands are recorded
into a command list and will be executed as a script with the `run`
command. Once entered, the `save` command is used inside the macro recording
environment to commit the test commands and leave the recording context.

list
----
    list <test-name>

Shows the program source for the test given. The source may not be exactly
the same as the original, as some features, like blocks, result in
automatically generated breakout labels.

run
---
    run <test-name>

Execute the named test. Tests currently loaded can be viewed with the
`tests` command

tests
-----
Shows the list loaded tests

Testing Environment Commands
============================
Here are the currently-supported commands in alphabetical order.

abort
-----
    abort [message]

Abort the test immediately. No further instructions (except any labels
registered with `atexit` will be executed. The test status will reflect
aborted.

If `message` is provided, it will be given with the indication that the test
was aborted.

atexit
------
    atexit <label>

Instructs the test environment to execute commands starting from the given
label. The last instruction to be executed in this label can optionally be
"return". Otherwise, execution will continue through the end of the script.
Any other "goto" and "call" instructions will still be interpreted as they
normally would be.

break
-----
    break

Used with the `for` and `while` loop constructs to exit the loop earlier
than what would normally cause loop to exit. For loops will exit when all
the items in the iterable have been exhausted. While loops will exit when
the provided condition evaluates to False. The loop closest to the break
statement found when unwinding the stack will be aborted. Script execution
will continue with the command following the loop.

call
----
    call <label-name> [with <var=value> ...]

The next command executed will be the first command following the given
label. The label is allowed to be defined after the `call` command. In other
words, forward declarations are allowed. Test execution will terminate here
if the label is not defined at runtime.

The current location of the test is saved on the stack. If a `return`
command is encountered, script execution will resume with the following
command.

The call command can be used to treat the label as a function by passing
arguments with the `with` statement. Following the `with` statement should
be a list of space-separated variable name and value pairs. The variables
will be defined to have the given value in the target label. Any valid
Python expression is welcome as the target value of a variable argument. If
the variable exists in the current scope, its value in the current scope
will not be modified by the `call` command.

The value of the `return` command can be captured if the `call` command is
used as a bracketed sub-command, for instance

    if [call safety-check with n=3]
        abort "No longer safe to operate"

continue
--------
    continue

Causes script execution flow to immediately resume at the top of the
previous loop found when unwinding the stack. While loops will still
evaluate the leading condition before continuing with the looped command or
block. For loops will continue with the next item in the iterable if there
is another. If a continue statement causes the loop to end, it would be the
same as a `break` statement.

count
-----
    count <counter-name>

Returns the current count value of the named counter

counter
-------
    counter <name> [filename]

Defines a increment-only counter that can be used for life-cycle testing.
Refer to the `inc` and `count` commands for working with counters.

If the `filename` argument is provided, the file will be used as a backer
for the counter value. When the test is run, the counter will load the value
from the file if possible, and after each increment, the value will be
written to the file. This will make the counter value persistent across test
executions.

debug
-----
    debug <on|off>

Enables or disables debug mode. in debug mode, each command is written to
the status output before it is executed.

defined
-------
    defined <name>

Outputs `true` if the name is defined in the context currently and `false`
otherwise.

do
--
Begins a block. Blocks are completed with the `done` command. Under the hood
(and you can inspect with the `list` command), blocks are rewritten to fall
under a random label. The `do` command is replaced with a `call` command,
and the `done` command is replaced with a `return` command.

Nested blocks are welcome.

done
----
Completes a block started with the `do` command. `done` will raise an error
if there is no complimentary `do` command associated.

expired
-------
    expired <timer-name>

Returns `true` if the timer has expired, and `false` if there is at least
one second remaining on the timer. Refer to `timeout` for creating timers.

for
---
    for <var> in <iterable>
        <command or block>

Executes the command or block following the for command for each item in the
iterable given. The named var is defined to point to each item in the
iteration and will also be valid as the last item in the iterable following
the completion of the for command.

fail
----
    fail [message]

Exits the test indicating failure. Any labels registered with `atexit` are
executed as usual. If `message` is provided, it is written to the output
with the failure indication.

goto
----
    goto <label-name>

The next command executed will be the first command following the given
label. The label is allowed to be undefined when the `goto` is encountered.
In other words, forward declarations are allowed. However, when the test is
executed, it will be aborted when this `goto` command is reached if the
label was never defined.

inc
---
    inc <counter-name>

Increments the named counter by one. Refer to `counter` for creating
counters.

if
--
    if <condition>
        <command>

Executes the following `command` if and only if the `condition` evaluates
to `true`. The `condition` can be any valid Python expression, however, no
Python built-in functions and types are valid. Therefore, comparing against
the Python value `True` is not valid. Use brackets to perform sub-commands
and evaluate their results.

The `command` can be placed on the same line with the if statement if a
semi-colon is used to separate the commands. **Note that this is a change
from the 0.1 software release.**

label
-----
    label <name>

Defines a label that can be the target of a "atexit", "goto" or "call"
command later

let
---
    let <name> = <expression>

Evaluate the given expression and assign the result to the name given. The
variable name given exists in the local environment afterwards and can be
used for variable substitution.

pause
-----
    pause [message]

Write the given message to the output and wait for the user to press the
[Enter] key. Any input given by the user is disregarded. Use the general
`read` command to read data from the user.

print
-----
    print [what]

Write the given message to the output. The message is evaluated as an
expression, so subcommands can be included in brackets. Also, any valid
Python expression is valid. For instance:

    print "Current reading is {0}".format([a -> port 1 read])

return
------
    return [expression]

Returns script execution to the command following the previously-encountered
`call` statement. If the call stack is empty, the script will be terminated
with an error.

The call stack is limited by the available memory in the Python runtime.
Ensure that your `call` and `return` commands are balanced.

If an expression is given, its value will be returned and will be the value
of the `call` command that invoked the current label. Any valid Python
expression is welcome.

save
----
    save

Exits macro record mode. When this command is encountered, no commands
following will be interpreted as part of the test.

succeed
-------
    succeed [message]

Exits the test indicating success. Any labels registered with `atexit` are
executed as usual.  If `message` is provided, it is included in the output
of the test indicating success.

task
----
Create or manages a task, with the following subcommands, where `<>` is the
name of the task to create or manage.

    task <> fork {label} [with var=value ...]
    task <> send [something]
    task <> join

### task <> fork {at} [with var=value ...]
Creates a new parallel task by forking execution at the given label or by
executing the named test in parallel. The label is defined by the `label`
command as usual, and a test is defined with the `create` command as usual.
Forked tasks do not start immediately, they are kicked off with the `task
start` command.

The current environment is also copied into the forked task. Therefore, the
current values of all local and global variables are copied and frozen in
the new task. Changes to variables and environment made in tasks are not
reflected in other tasks with one caveat. Changes to complex variables such
as lists and dicts are reflected, because the same object is being operated
on in the task.

The task will start executing in parallel immediately and can be controlled
with the `task send` command if the task makes use of the `yield` command.

Initial environment (local variable scope) can be controlled with the `with`
keyword followed by space-separated keyword arguments for the tasks initial
scope.

### task <> join
Suspends the calling task until the joined task completes. Completion is as
usual, where the task executes until the end of the script, or reaches a
`succeed` or `fail` command.

The state of the joined task is also synchronized with the join command, so
if the joined task failed or aborted, it will cause the joining task to
also fail or abort respectively.

### task <> send [something]
Sends data into a task when the task reaches a `yield` command. If the task
has not yet reached a `yield` command, the sending task will be suspended
until the task reaches a `yield` command. If the receiving task does not
reach a `yield` command, the send will block until the task exits, which
would be the same as a join.

Send can be used in a few manners:

Allow the task to continue past a `yield` command. This will provide for
something like checkpoint based synchronization

    task <> send

Send some data to the task at a `yield` command and allow the task to
continue to the following `yield` command or completion

    task <> send 42

Receive data from the task and allow it to continue on from its current
`yield` command

    let output = [task <> send]

Full duplex communication and synchronization, allowing the receiving task
to continue onward to the following `yield` command

    let output = [task <> send 42]

Any valid Python expression can be sent with the `task send` and `yield`
commands.

Note again that `task send` will cause the sender to suspend until the
receiving task reaches a `yield` command and visa versa.

timeout
-------
    timeout <timer-name> [time=0]

Defines a timer to expire after the given number of seconds (`time`). If
time is not specified the timer will begin expired. Timers can be reloaded
by using the `timeout` command with the same `timer-name`. Refer to the
`expired` command for checking the time remaining on the timer.

yield
-----
    yield [something]

If used as a subcommand, input is possible

    let input = [yield something]
    let input = [yield]

Suspends a task until data is sent to it via the `task send` command. If not
used as a subcommand, the received data is discarded; however, send will
always cause the task to resume and continue to completion or the following
`yield` command.

Yield is also used to produce data for an outside task. Any valid Python
expression is valid as the argument to the `yield` command, and its value
will be received as the result of a `task send` command, if it is used as a
subcommand.

wait
----
    wait <time>

Pauses execution for the amount of time specified, in seconds. Fractional
seconds can be expressed as a floating point number.

while
-----
    while <condition>
        <command or block>

Executes the following command or block as long as the provided condition
evaluates to True. If the condition evaluates to True initially, the
following command or block will not be executed. As with the `for` loop,
`break` and `continue` commands can be used to alter the normal control flow
of the loop.
