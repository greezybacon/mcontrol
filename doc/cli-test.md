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

call
----
    call <label-name>

The next command executed will be the first command following the given
label. The label is allowed to be defined after the `call` command. In other
words, forward declarations are allowed. Test execution will terminate here
if the label is not defined at runtime.

The current location of the test is saved on the stack. If a `return`
command is encountered, script execution will resume with the following
command.

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
    return

Returns script execution to the command following the previously-encountered
`call` statement. If the call stack is empty, the script will be terminated
with an error.

The call stack is limited by the available memory in the Python runtime.
Ensure that your `call` and `return` commands are balanced.

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

timeout
-------
    timeout <timer-name> [time=0]

Defines a timer to expire after the given number of seconds (`time`). If
time is not specified the timer will begin expired. Timers can be reloaded
by using the `timeout` command with the same `timer-name`. Refer to the
`expired` command for checking the time remaining on the timer.

wait
----
    wait <time>

Pauses execution for the amount of time specified, in seconds. Fractional
seconds can be expressed as a floating point number.
