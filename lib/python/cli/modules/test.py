# coding: utf-8
"""
Testing framework

Provides the commands and context to write and reuse testing script
"""
from __future__ import print_function

from . import Mixin, trim

from .. import Shell

import cmd
import random
import shlex
import string

import sys
# Python3k clobbered 'input' and 'raw_input'
if sys.version_info >= (3,0):
    raw_input = input

class TestingCommands(Mixin):

    def do_create(self, line):
        """
        Enters a test creation context. From the context, new motors can
        still be connected with the connect command. In the test creation
        context, commands entered are not executed immediately. Instead,
        they are saved into a test script that is run with the 'run'
        command.

        Usage:

        create <name>
        """
        if line == "":
            raise SyntaxError("Test name must be specified")

        test = TestingSetupContext(context=self.context.copy())
        test.cmdqueue = self.cmdqueue
        test.cmdloop()
        self.context['tests'][line] = test

    def do_run(self, name):
        """
        Executes the given test.

        Usage:

        run <name>

        Where <name> is the name of an already-created test script. To
        create scripts. See the 'help create' command
        """
        if not name:
            return self.error("Specify the name of the test to run")
        elif name not in self.context['tests']:
            return self.error("{0}: Test not yet created")

        test = TestingRunContext(test=self.context['tests'][name],
                context=self.context.copy())
        test.run()

        if test.state == test.Status.ABORTED:
            self.warn("Test was aborted")
        elif test.state == test.Status.FAILED:
            self.error("Test reports failure")
        elif test.state == test.Status.SUCCEEDED:
            self.status(">>> Test completed successfully")
        else:
            self.warn("Test aborted unexpectedly")

    def complete_run(self, text, line, begidx, endix):
        return [x for x in self['tests'].keys() if x.startswith(text)]

    def do_tests(self, ignored):
        """
        Displays a list of all loaded test scripts
        """
        for test in self.context['tests']:
            self.out(test)

    def do_list(self, test):
        """
        List the compiled source code for a `create`d script. The listed
        source will not be exactly the same as the given source, as some
        commands are rewritten at compile-time.
        """
        if not test in self['tests']:
            self.error('{0}: Test not defined',
                'Use "tests" for a list of defined tests')
            return
        labels = dict((k,v) for v, k in self['tests'][test].labels.items())
        indent = 0
        for i, x in enumerate(self['tests'][test]):
            if i in labels:
                self.status("label {0}".format(labels[i]))
                indent = 4
            self.status(" " * indent + x)
    complete_list = complete_run

import os
class Counter(object):
    """
    Counter instrument used to record the number of counts a testing loop
    was performed. The counter can be backed by a file so that if the test
    is restarted and the same file is used, the last-recorded count will be
    used as the initial count
    """
    def __init__(self, filename=False):
        if filename and not os.path.exists(os.path.dirname(filename)):
            raise ValueError("Folder for counter file does not exist")
        self.filename = filename
        self.count = 0
        try:
            if filename:
                self.count = int(open(filename, "rt").read())
        except:
            # TODO: Emit a warning about inability to read counter value
            pass

    def increment(self):
        self.count += 1
        if self.filename:
            open(self.filename, 'wt').write(str(self.count))

import time
class Timer(object):
    """
    Timer instrument used to perform testing for a duration of time
    """

    def __init__(self):
        self.start = 0
        self.seconds = 0

    def timeout(self, seconds):
        self.seconds = seconds
        return self.restart()

    def restart(self):
        self.start = time.time()
        return self

    @property
    def elapsed(self):
        return int(time.time() - self.start)

    @property
    def remaining(self):
        return max(0, self.seconds - self.elapsed)

class TestingSetupContext(cmd.Cmd):

    prompt = "recording $ "
    intro = ""

    def __init__(self, *args, **kwargs):
        cmd.Cmd.__init__(self)
        self.test = []
        self.labels = {}
        self.blocks = {}
        self.atexit = []
        # Stack of blocks created by the `do` and `then` commands. Stack
        # allows for nested blocks
        self.block_stack = []

    def default(self, line):
        """
        Creates an instruction to be processed later when the test is
        executed.

        Use motor names followed by an arrow to indicate which motor the
        condition is to be against:

        a -> move to 1 inch
        b -> move 33 inch
        a -> wait
        b -> wait
        """
        # Drop comments
        line = re.sub(r'#.*$', '', line)
        # Skip empty lines
        if not re.match(r'^\s*$', line):
            # TODO: Basic evaluation of the command. Sanity checks, overflow to
            #       following line, etc.
            #
            # Split by ';' and ':' to form multiple commands and append each
            # one separately
            parts = self.split(line)
            if len(parts) > 1:
                self.cmdqueue[0:0] = parts
            else:
                self.emit(line)

    def emit(self, line):
        if len(self.block_stack):
            self.block_stack[-1].append(line)
        else:
            self.test.append(line)

    def split(self, line):
        # Thanks, http://stackoverflow.com/a/2787064
        if ';' not in line:
            return [line]
        return re.split(r'''((?:[^;"']|"[^"]*"|'[^']*')+)''', line)[1::2]

    def postloop(self):
        if len(self.block_stack):
            print("Unclosed block. do found without closing done",
                "See 'help do'")
            return True
        # Ensure that the blocks are not executed without a 'call' command
        self.test.append('succeed')
        for label, code in self.blocks.items():
            self.labels[label] = len(self.test)
            self.test.extend(code)
        self.blocks = {}

    def error(self, what, help=''):
        print("*** " + what + '. ' + help)
        return True

    def do_label(self, name):
        """
        Creates a label that can be jumped to later.

        Usage:

        label <name>
        """
        if ' ' in name:
            return self.error("Invalid name -- only non-whitespace chars")
        self.labels[name] = len(self.test)

    def do_save(self, line):
        return True

    def do_atexit(self, line):
        """
        Defines a label which should be run at the exit of the script.
        Regardless of how the script terminates, whether by success, fail,
        abort, or unhandled error, the atexit labels will be (attempted to
        be) executed.

        The labels will be executed with the 'call' commands (see 'help
        call'), so to prevent execution from flowing into the following
        label, use the 'return' command to exit the label.

        Example usage:

        atexit cleanup

        label main-loop
            ...
            succeed

        label cleanup

            # Turn off the pump before completing the test
            a -> port 1 set low

            return
        """
        line = line.strip()
        if len(line) == 0:
            return self.error("Incorrect usage", "See 'help atexit'")

        if line not in self.atexit:
            self.atexit.append(line)

    def do_do(self, what):
        """
        Allows for the creation of arbitrary blocks of code. The `do`
        instruction will setup an anonymous block of code that will be
        `call`ed when the block is invoked. This will allow for more complex
        operations to be the target of an `if` statement and will allow for
        better integration of `elif` and `else` statements
        """
        # Pick a random label name, then, scan the code down to the `done`
        # keyword and move the code into the label block. Finally, add an
        # unconditional `return` statment to the block and replace the code
        # in the test instructions with a call to the anonymous block
        while True:
            label = ''.join(random.choice(string.letters) for x in range(8))
            if label not in self.labels:
                break
        # Emit the `call` command to call the block before the block is
        # started, so that it will be emitted into the current block
        self.emit('call {0}'.format(label))
        self.block_stack.append([])
        self.blocks[label] = block = self.block_stack[-1]
        if len(what.strip()):
            block.append(what)

    def do_done(self, what):
        """
        Completes a block defined by the `do` command
        """
        if not len(self.block_stack):
            print("Unbalanced block. done found without do",
                "See 'help do'")
            return True
        block = self.block_stack.pop()
        block.append('return')

    def emptyline(self):
        pass

    def __iter__(self):
        return iter(self.test)

class OutputCapture(object):
    """
    File-like object used to trap output sent through a shell context's
    ::out() method
    """
    def __init__(self):
        self.writes = []

    def write(self, what):
        if what is OutputCapture.Flush:
            self.writes = []
        else:
            self.writes.append(what)

    def as_result(self):
        if len(self.writes) == 1:
            return repr(self.writes[0])
        else:
            return self.writes

    def isatty(self):
        return False

OutputCapture.Flush = object()
class LoopControl(Exception): pass
class Continue(LoopControl): pass
class Break(LoopControl): pass

import cmd
import re
class TestingRunContext(Shell):

    class Status(object):
        READY = 1
        RUNNING = 2
        SUCCEEDED = 3
        FAILED = 4
        ABORTED = 5

    def __init__(self, test, *args, **kwargs):
        Shell.__init__(self, *args, **kwargs)
        self.test = test
        self.instructions = list(self.test)
        self.context['counters'] = {}
        self.context['labels'] = []
        self.context['timers'] = {}
        self.stack = []
        self.vars = self.context['env']
        self.debug = False
        self.state = self.Status.READY
        self.context['tasks'] = {}
        self.taskname = ""

    def onecmd(self, str):
        try:
            return cmd.Cmd.onecmd(self, str)
        except KeyboardInterrupt:
            self.do_abort(None)
        except LoopControl:
            raise
        except Exception as e:
            self.error("{0}: {1}: (Unhandled) {2}".format(
                str, type(e).__name__, e))
            return True

    def error(self, message, hint=""):
        self.state = self.Status.ABORTED
        return super(TestingRunContext, self).error(message, hint)

    def parse(self, command):
        """
        Parses a command and yields a tuple of the motor and the command.
        If the command is not targeted at a motor, the motor item of the
        tuple will be None
        """
        match = re.match(r'(?:(?P<motor>[\w*!^]+)\s*->\s*)?(?P<cmd>.*$)',
            command.format(**self.vars))
        return (match.groupdict()['motor'], match.groupdict()['cmd'])

    def execute(self, command, capture=False):
        """
        Executes the command in the current context of this shell, if no
        motor is indicated as the target of the command. If the command
        indicates it's targeted at a motor, the command will be executed in
        the context of that motor.

        The output of the command is captured and returned if the capture
        parameter is set. So anything sent to the self.out() function in the
        shell context the command is running in will be returned from this
        method. Otherwise, the value returned from the executed command is
        returned.
        """
        motor, command = self.parse(command)
        context = self if not motor else self.context['motors'][motor]
        if self.debug:
            if motor:
                self.status("EXEC[{0}]:{3}: {1} -> {2}".format(len(self.stack),
                    motor, command, self.taskname))
            else:
                self.status("EXEC[{0}]:{2}: {1}".format(len(self.stack),
                    command, self.taskname))

        # Capture the output of the command
        if capture:
            stdout = context['stdout']
            context['stdout'] = OutputCapture()

        try:
            context['ctrl-c-abort'] = True
            result = context.onecmd(command.format(**self['env']))
            context['ctrl-c-abort'] = False
        except KeyboardInterrupt:
            # User interrupted -- abort the test
            if self.debug:
                self.status("CTRL+C -- Aborting")
            return self.do_abort(None)
        except LoopControl:
            raise
        except Exception as ex:
            return self.error("Unexpected Error: {0!r}".format(ex))

        if capture:
            result = context['stdout'].as_result()
            context['stdout'] = stdout
            if self.debug:
                self.status("{0} => {1}".format(command, result))

        # TODO: Return value from stdout after bugging stdout to retrieve
        #       data written through self.out()
        return result

    def expand(self, text):
        """
        Replaces bracket expressions with the results of the commands
        """
        return re.sub(r'\[(?P<command>[^][]+)\]',
            lambda match: self.execute(match.group(1), capture=True),
            text)

    def eval(self, expression):
        """
        Evaluates the Python expression and returns the result
        """
        # First off, search for bracket expressions
        expression = self.expand(expression)
        try:
            return eval(expression, {}, self.vars)
        except SyntaxError:
            self.error("Syntax Error", expression)

    def do_counter(self, line):
        """
        Use a file to store and retrieve a counter. Usually this is used to
        keep track of a number of cycles in a restartable fashion. If a file
        is specified, the count will persist between test executions

        Usage: counter <name> [<filename>]

        To display the counter, use 'count <name>'. To increment the count,
        use 'inc <name>'
        """
        parts = line.split()
        if len(parts) < 1:
            return self.error("Invalid usage", "See 'help counter'")

        name = parts.pop(0)
        filename = parts.pop(0) if len(parts) else False

        self.context['counters'][name] = Counter(filename)

    def do_inc(self, name):
        """
        Increment a loaded counter by name. To load a counter, see 'help
        counter'
        """
        if not name in self.context['counters']:
            return self.error("Counter not yet loaded",
                "See 'help counter'")
        self.context['counters'][name].increment()

    def do_count(self, name):
        """
        Display the current value of the named counter
        """
        if not name in self.context['counters']:
            return self.error("Counter not yet loaded",
                "See 'help counter'")
        self.out(self.context['counters'][name].count)

    def do_timeout(self, name):
        """
        Creates a timer that can be (re)loaded and then checked for
        remaining time and expiration.

        Usage:

        timeout onehour [3600]
        """
        seconds = 0
        if ' ' in name:
            name, seconds = name.split()
        self.context['timers'][name] = Timer().timeout(int(seconds))

    def do_expired(self, name):
        """
        Outputs True if the timer has expired and False otherwise.

        Usage:

        expired onehour
        """
        self.out(self.context['timers'][name].remaining == 0)

    def do_if(self, line):
        """
        Conditional statement. Otherwise same as any other statement

        Usage:

        if <condition>; <statement>

        If the condition evaluates to a boolean True value, then the given
        statement will be executed.

        Sub commands can be used if enclosed in the usual brackets. Otherwise,
        any valid Python expression is supported in the condition.

        if [a -> get stalled]; abort
        """
        # Evaluate the condition
        if not self.eval(line):
            # Skip the next command (the target of the if command)
            self.next += 1

    def do_while(self, condition):
        """
        Continues to execute the following command or block as long as the
        provided condition evaluates to a boolean True. Inside the following
        block, `break` and `continue` can be used to short-circuit the
        normal control flow.

        Usage:
            while <condition>; do
                something
            done
        """
        if len(condition.strip()) == 0:
            return self.error("while: Incorrect Syntax", "See 'help while'")
        # Ensure instruction pointer and stack base are preserved
        next = self.next
        depth = len(self.stack)
        while self.eval(condition):
            if self.state != self.Status.RUNNING:
                break
            try:
                self.execute_script(next+1, count=1)
            except Break:
                break
            except Continue:
                pass
        self.next = next + 1
        self.stack = self.stack[:depth]

    def do_for(self, what):
        """
        Executes the following command or block for every item in the given
        iterable. Inside the following block, `break` and `continue` can be
        used to short-circuit the normal control flow of the commands.

        Usage:
            for <var> in <iterable>; do
                print str(var)
            done
        """
        parts = what.split(' ', 2)
        if len(parts) != 3:
            return self.error("for: Incorrect Syntax", "See 'help for'")
        var = parts.pop(0)
        if parts[0] != 'in':
            return self.error("for: Incorrect Syntax: expected 'in'",
                "See 'help for'")

        iterable = self.eval(parts[1])
        # Ensure instruction pointer and stack base are preserved
        next = self.next
        depth = len(self.stack)
        for x in iterable:
            if self.state != self.Status.RUNNING:
                break
            try:
                self.vars[var] = x
                self.execute_script(next+1, count=1)
            except Break:
                break
            except Continue:
                pass
        self.next = next + 1
        self.stack = self.stack[:depth]

    def do_abort(self, line):
        """
        Abort the test immediately. No other statements will be executed and
        the test state will be marked as ABORTED
        """
        if line and len(line):
            self.warn(self.eval(line))
        self.state = self.Status.ABORTED
        return True

    def do_succeed(self, line):
        """
        Succeed the test immediately. No other statements will be executed
        and the test state will be marked as SUCCEEDED
        """
        if line and len(line):
            self.status(self.eval(line))
        self.state = self.Status.SUCCEEDED
        return True

    def do_fail(self, line):
        """
        Fail the test immediately. No other statements will be executed and
        the test state will be marked as FAILED
        """
        if line and len(line):
            self.error(self.eval(line))
        self.state = self.Status.FAILED
        return True

    def do_goto(self, name):
        """
        Advances the test to the statement immediately following the named
        label
        """
        if name not in self.test.labels:
            return self.error("{0}: Undefined label".format(name))
        self.next = self.test.labels[name] - 1

    def do_call(self, name):
        """
        Like goto, except that the current location is stored, so that, if a
        'return' statement is encountered, the test will continue with the
        statement following this one.

        Optionally arguments can be passed which will be added to the
        environment before the call and will be reset after the call. Any
        valid expression is acceptable as the value of the variable passed.
        The usual bracketed expressions as well as any valid Python
        expression can be used.

        call function with var=value
        """
        self.stack.append(self.next)
        parts = name.split()
        name = parts.pop(0)
        if name not in self.test.labels:
            return self.error("{0}: Undefined label".format(name))

        # Handle arguments passed to the function call
        saved = {}
        if 'with' in parts:
            for x in parts[1:]:
                if '=' not in x:
                    return self.error("Function arguments must be keywords",
                        "See 'help call'")
                var, val = x.split('=', 1)
                if var in self.vars:
                    saved[var] = self.vars[var]
                self.vars[var] = self.eval(val)

        # Execute the block and restore the previous name context
        self.execute_script(start=self.test.labels[name])
        self.next = self.stack.pop()
        self.vars.update(saved)

        # If a fail / succeed / abort was invoked bail at this level
        return self.state != self.Status.RUNNING

    def do_return(self, line):
        """
        Returns to the statement immediately following the previous 'call'
        statement. If an argument is received the evaluated result is sent
        to the output. Therefore, return can be used in a manner similar to
        traditional functions:

        let var = [call function]
        succeed {var}

        label function
            return 13
        """
        if len(self.stack) == 0:
            return self.error("Unbalanced stack: Return without call")
        if line:
            self.out(OutputCapture.Flush)
            self.out(self.eval(line))
        return True

    def do_continue(self, ignored):
        """
        Used inside a loop to continue the loop fresh from the top. If
        continue is used inside a block of function inside a loop, the loop
        at the closest outer level will intercept the `continue` command
        """
        if len(ignored.strip()):
            return self.error("continue: Incorrect syntax",
                "See 'help continue'")
        raise Continue

    def do_break(self, ignored):
        """
        Used inside a loop to abort out of the loop. If `break` is used
        inside a block or function call, the closest enclosing loop will
        receive the `break` signal
        """
        if len(ignored.strip()):
            return self.error("break: Incorrect syntax", "See 'help break'")
        raise Break

    def do_print(self, what):
        """
        Outputs the value of the evaluated expression. The expression is
        evaluated in the context of the current test. So any local variabled
        defined with the 'let' statements may be used. Subcommands are also
        supported if enclosed in brackets.

        Usage:

        let var = 13
        print var
        print [a->get stalled]
        """
        self.status(self.eval(what))

    def do_eval(self, what):
        """
        Evaluate the rest of the line as a Python expression. Eval can be
        used as a subcommand as well. Note that only expressions are
        supported. Python control structures like 'for' and 'import' are not
        valid.

        Usage:

        eval list_var.append(value)
        print [eval value + 32]
        """
        self.out(self.eval(what))

    def do_pause(self, what):
        """
        Outputs a message and waits for the user to press the [Enter] key
        """
        raw_input(self.eval(what or "'Press Enter to continue '"))

    def do_label(self, line):
        # Handled at compile time in the Setup context.
        pass

    def do_atexit(self, line):
        # Handled at compile time
        pass

    def do_let(self, line):
        """
        Create or modify a local variable. Any valid python expression is
        valid. Commands to execute locally or by a motor should be enclosed
        in the usual brackets.

        Usage:

        let stalled = [a -> get stalled] or [b -> get stalled]
        """
        if not '=' in line:
            return self.error("Incorrect usage", "See 'help let'")
        name, expression = line.split('=', 1)
        self.vars[name.strip()] = self.eval(expression)

    def do_defined(self, line):
        """
        Used in an if condition to determine if a variable has be set by
        either a 'read' statement or a 'let' statement.

        Usage:

        if [defined varname]: do something
        """
        name = line.strip()
        if len(name) == 0:
            return self.error("Incorrect usage", "See 'help defined'")
        self.out(name in self.vars)

    def do_debug(self, state):
        """
        Enable debug output. All executed commands are sent to output. Valid
        debug states are 'on' and 'off'

        Usage:

        debug on -or- debug off
        """
        if state.lower() not in ('on','off'):
            return self.error("{0}: Invalid debug state",
                "See 'help debug'")
        self.debug = state.lower() == 'on'

    def do_wait(self, line):
        """
        Wait for some amount of time, in seconds

        Usage:
        wait 0.5

        will wait for half a second
        """
        try:
            time.sleep(float(line))
        except ValueError:
            return self.error("Incorrect wait time", "See 'help wait'")

    def do_task(self, line):
        """
        Create a task from a label. Tasks inherit the current runtime
        environment when invoked.

        Subcommands:
            <> fork {label} Create a task from the given label
            <> start [with var=val ...]
                            Kickoff the task, with optional environment
            <> join         Wait for the task to complete
            <> send {what}  Send something to the task

        Where <> represents the name of the target task

        Usage:
            task <> fork label1
            task <> start
            task <> join

        Communication:
            Tasks can be configured to operate around data in and output.
            Inside the task, the `yield` command is used to send and receive
            data. Outside the task, the `send` and `read` subcommands are
            used to communicate with the task.

            task <> send                     # Read from `yield`
            task <> send 42                  # Send to `yield`
            let output = [task <> send 42]   # Send/read to/from `yield`
        """
        parts = line.split()
        if len(parts) < 2:
            return self.error("task: incorrect usage",
                "See 'help task'")
        name = parts.pop(0)
        command = parts.pop(0)
        if command == 'fork':
            if len(parts) != 1:
                return self.error("task: create: label name is the only argument",
                    "See 'help task'")
            if parts[0] not in self.test.labels:
                return self.error("task: {0}: Label does not exist"
                    .format(parts[0]))
            self['tasks'][name] = Task(runtime=self,
                start=self.test.labels[parts[0]], name=name, **self.vars)

        elif name not in self['tasks']:
            return self.error("task: {0}: Task not yet created".format(name))

        task = self['tasks'][name]
        if command == 'start':
            # TODO: Handle keyword arguments as environment
            task.start()
        elif command in ('join', 'send'):
            if command == 'join':
                task.join(100000) # Timeout required for interruption
            else:
                what = self.eval(' '.join(parts)) if len(parts) else None
                message = task.send(what)
                if type(self['stdout']) is OutputCapture:
                    self.out(message)
            if task.state not in (self.Status.SUCCEEDED, self.Status.RUNNING):
                self.state = task.state
                return True

    def do_yield(self, what):
        """
        Yield execution of this task until another item is read from this
        task. Yield is only valid inside a task. See 'help task' for
        information on tasks. If yield is used in a bracketed subcommand,
        the value given in `task <> send` is returned.

        Yield is used to pause the execution of a task, to pass information
        to a parent or co-task, and to pause until data is ready to be sent
        into the task.

        Usage:
            yield                       # Wait until task send
            yield <what>                # Output and wait for send
            let input = [yield]         # Wait and capture data from send
            let input = [yield <what>]  # Output and capture data from send

        Where <what> is the value to be yielded to the sending task and is
        always optional. Python None is yielded if nothing is given
        """
        return self.error("yield: only valid in tasks", "See 'help task'")

    def do_atexit(self, line):
        # This is handled at compile time
        pass

    def execute_script(self, start=0, count=-1):
        self.next = start
        while self.state == self.Status.RUNNING \
                and count and self.next < len(self.instructions):
            # Evaluate each command. Goto commands will change self.next, so
            # don't do a simple iteration of the instructions
            if self.execute(self.instructions[self.next]):
                break
            if self.next is None:
                break
            self.next += 1
            count -= 1

    def cmdloop(self):
        self.state = self.Status.RUNNING

        # Execute the main script
        self.execute_script(0)

        for atexit in self.test.atexit:
            # Advance the script to the location of the atexit label and
            # execute the script from there. If a 'return' is encountered,
            # leave a 'None' on the stack to signal the script should not be
            # continued.
            self.stack.append(None)
            self.execute_script(self.test.labels[atexit])

        # Assume success if not otherwise set
        if self.state == self.Status.RUNNING:
            self.state = self.Status.SUCCEEDED

        for task in self['tasks'].values():
            task.state = self.state
            task.exit()

    postloop = Shell.halt_all_motors

Empty = object()
import threading
class Task(TestingRunContext, threading.Thread):
    """
    Provides a simple mechanism for running parallel tasks. Tasks can be
    created in the runtime context with an assignment and subcommand such as

    Tasks can behave like Python generators/coroutines, so you can send()
    values to them and yield() values from them.

    Tasks inherit the context from the originating runtime; however, changes
    made to variables inside the task context are not reflected outside the
    context to the original runtime. Therefore, traditional thread locking
    mechanics are not (necessarily) required.
    """
    def __init__(self, runtime, start, name, **context):
        super(Task, self).__init__(runtime.test)
        threading.Thread.__init__(self)
        self.vars = context
        self.context = runtime.context.copy()
        self.starting = start
        self.yieldcond = threading.Condition()
        self.sendcond = threading.Condition()
        self.daemon = True
        self.inkitty = Empty
        self.outkitty = Empty
        self.taskname = name

    def run(self, **kwargs):
        self.vars.update(kwargs)
        self.state = self.Status.RUNNING
        self.execute_script(start=self.starting)
        # Free a sender if yield was never reached
        self.exit()

    def exit(self):
        # Free from a [yield] command
        with self.sendcond:
            self.inkitty = self.outkitty = None
            self.sendcond.notify()
        with self.yieldcond:
            self.yieldcond.notify()

    def do_yield(self, what):
        with self.yieldcond:
            self.outkitty = self.eval(what) if what.strip() else None
            self.yieldcond.notify()
        # Wait until [send] is called
        with self.sendcond:
            while self.inkitty is Empty:
                self.sendcond.wait()
            if type(self['stdout']) is OutputCapture:
                self.out(self.inkitty)
            self.inkitty = Empty

    def send(self, what):
        # Wait until [yield] is called first
        with self.yieldcond:
            while self.outkitty is Empty:
                self.yieldcond.wait()
            retval = self.outkitty
            self.outkitty = Empty
        with self.sendcond:
            self.inkitty = what
            self.sendcond.notify()
        return retval

# Add help for the commands in the Run context into the setup context
for func in dir(TestingRunContext):
    if func.startswith('do_'):
        item = getattr(TestingRunContext, func)
        def help(docstring):
            def help_output(self):
                print(trim(docstring))
            return help_output
        setattr(TestingSetupContext, 'help_{0}'.format(func[3:]),
            help(item.__doc__))
