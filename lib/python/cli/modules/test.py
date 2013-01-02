# coding: utf-8
"""
Testing framework

Provides the commands and context to write and reuse testing script
"""
from __future__ import print_function

from . import Mixin, trim

from cli import Shell

class TestingCommands(Mixin):

    def do_create(self, line):
        """
        Enters a test creation context. From the context, new motors can
        still be connected with the connect command. In the test creation
        context, commands entered are not executed immediately. Instead,
        they are saved into a test script that is run with the 'run'
        command.
        """
        if line == "":
            raise SyntaxError("Test name must be specified")

        test = TestingSetupContext(context=self.context.copy())
        test.run()
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
                context=self.context)
        test.run()

        if test.status == test.Status.ABORTED:
            self.warn("Test was aborted")
        elif test.status == test.Status.FAILED:
            self.error("Test reports failure")
        elif test.status == test.Status.SUCCEEDED:
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

class TestingSetupContext(Shell):

    prompt_text = "recording $ "
    intro = ""

    def __init__(self, *args, **kwargs):
        Shell.__init__(self, *args, **kwargs)
        self.test = []
        self.labels = {}

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
        if re.match(r'^\s*$', line):
            return
        # TODO: Basic evaluation of the command. Sanity checks, overflow to
        #       following line, etc.
        self.test.append(line)

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
        self.writes.append(what)

    def as_result(self):
        if len(self.writes) == 1:
            return repr(self.writes[0])
        else:
            return self.writes

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
        self.context['counters'] = {}
        self.context['labels'] = []
        self.context['timers'] = {}
        self.stack = []
        self.test = test
        self.vars = {}
        self.debug = False
        self.status = self.Status.READY

    def parse(self, command):
        """
        Parses a command and yields a tuple of the motor and the command.
        If the command is not targeted at a motor, the motor item of the
        tuple will be None
        """
        match = re.match(r'(?:(?P<motor>\w+)\s*->\s*)?(?P<cmd>.*$)', command)
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
                print("EXEC: {0} -> {1}".format(motor, command))
            else:
                print("EXEC: {0}".format(command))

        # Capture the output of the command
        if capture:
            stdout = context['stdout']
            context['stdout'] = OutputCapture()

        try:
            result = context.onecmd(command)
        except KeyboardInterrupt:
            # User interrupted -- abort the test
            return self.do_abort(None)

        if capture:
            result = context['stdout'].as_result()
            context['stdout'] = stdout

        # TODO: Return value from stdout after bugging stdout to retrieve
        #       data written through self.out()
        return result

    def expand(self, text):
        """
        Replaces brace expressions with the results of the commands
        """
        return re.sub(r'\{(?P<command>[^{}]+)\}',
            lambda match: self.execute(match.group(1), capture=True),
            text)

    def eval(self, expression):
        """
        Evaluates the Python expression and returns the result
        """
        # First off, search for brace expressions
        expression = self.expand(expression)
        return eval(expression, {}, self.vars)

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

        if <condition>: <statement>

        If the condition evaluates to a boolean True value, then the given
        statement will be executed.

        Sub commands can be used if enclosed in the usual braces. Otherwise,
        any valid Python expression is supported in the condition.

        if {a -> get stalled}: abort
        """
        # Isolate the condition and statement
        # Evaluate the condition
        # Execute the statement
        match = re.match(r'(?P<cond>[^:]+):(?P<stmt>.*)', line)
        if not match:
            return self.error("Incorrect <if> usage.", "See 'help if'")
        if self.eval(match.groupdict()['cond']):
            return self.execute(match.groupdict()['stmt'])

    def do_abort(self, line):
        """
        Abort the test immediately. No other statements will be executed and
        the test status will be marked as ABORTED
        """
        self.status = self.Status.ABORTED
        return True

    def do_succeed(self, line):
        """
        Succeed the test immediately. No other statements will be executed
        and the test status will be marked as SUCCEEDED
        """
        self.status = self.Status.SUCCEEDED
        return True

    def do_fail(self, line):
        """
        Fail the test immediately. No other statements will be executed and
        the test status will be marked as FAILED
        """
        self.status = self.Status.FAILED
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
        statement following this one
        """
        self.stack.append(self.next)
        return self.do_goto(name)

    def do_return(self, line):
        """
        Returns to the statement immediately following the previous 'call'
        statement
        """
        self.next = self.stack.pop()

    def do_print(self, what):
        """
        Outputs the value of the evaluated expression. The expression is
        evaluated in the context of the current test. So any local variabled
        defined with the 'let' statements may be used. Subcommands are also
        supported if enclosed in braces.

        Usage:

        let var = 13
        print var
        print {a->get stalled}
        """
        self.out(self.eval(what))

    def do_label(self, line):
        # Handled at compile time in the Setup context.
        pass

    def do_let(self, line):
        """
        Create or modify a local variable. Any valid python expression is
        valid. Commands to execute locally or by a motor should be enclosed
        in the usual braces.

        Usage:

        let stalled = {a -> get stalled} or {b -> get stalled}
        """
        if not '=' in line:
            return self.error("Incorrect usage", "See 'help let'")
        name, expression = line.split('=', 2)
        self.vars[name.strip()] = self.eval(expression)

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

    def run(self):
        self.next = 0
        instructions = list(self.test)
        self.status = self.Status.RUNNING
        while self.next < len(instructions):
            # Evaluate each command. Goto commands will change self.next, so
            # don't do a simple iteration of the instructions
            if self.execute(instructions[self.next]):
                break
            self.next += 1
        # Assume success if not otherwise set
        if self.status == self.Status.RUNNING:
            self.status = self.Status.SUCCEEDED

# Add help for the commands in the Run context into the setup context
for func in dir(TestingRunContext):
    if func.startswith('do_'):
        item = getattr(TestingRunContext, func)
        def help(docstring):
            def help_output(self):
                self.status(trim(docstring))
            return help_output
        setattr(TestingSetupContext, 'help_{0}'.format(func[3:]),
            help(item.__doc__))
