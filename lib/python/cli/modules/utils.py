# coding: utf-8
"""
Utility functions placed here to keep the main cli.py a little more tidy
"""
from __future__ import print_function

from . import Mixin

import os
import shlex
import sys

class UtilityCommands(Mixin):

    def do_read(self, line):
        """
        Reads a variable from the user after presenting a prompt. The
        variable is placed into the environment of the current context.
        Commands processed after this variable is read may use braces in
        them to have the user input placed into the command.

        Usage:

        read "Prompt" into var

        Substitution into another command:

        connect mdrive:///dev/ttyM{var}:b
        """
        parts = shlex.split(line)
        if 'into' not in parts:
            return self.error("Incorrect usage", "See 'help read'")

        parts.remove('into')

        if len(parts) != 2:
            return self.error("Incorrect usage", "See 'help read'")

        prompt, variable = parts

        if sys.version_info[:2] < (3,0):
            # Python3 renames raw_input -> input
            input = raw_input

        # Show the prompt and read the var
        self['env'][variable] = input(prompt)

    def do_print(self, what):
        """
        Prints a message to the output. Useful for extra instructions
        embedded in script files

        Usage:

        print Message here, quotes are not required
        """
        self.status(what)

    def do_source(self, filename):
        """
        Executes all the commands in the given filename in the current shell
        context. Technically this works by adding all the commands in the
        file to the command queue of the current shell context.

        If there are other commands currently queued in this context, those
        commands will be executed after the newly-sourced commands. This
        allows scripts to include other scripts and assume that the contents
        of the subly-included scripts are executed before execution advances
        past the "source" command.

        As a caveat, no circular dependence checking is performed in
        sourcing.

        Usage:

        source <filename>
        """
        if not filename:
            return self.error("Specify filename of script to source")
        elif not os.path.exists(filename):
            return self.error("{0}: Script does not exist")

        self.cmdqueue = list(open(filename, 'rt').readlines()) + self.cmdqueue

