from __future__ import print_function

from . import overloaded
from .compiler import Compiler
from .grammar import language

from .pyPEG import parse
from ast import literal_eval
import fileinput
import operator
import sys
from functools import reduce

class Parser(object):

    def __init__(self, environ=None):
        self.r = []
        self.env = environ or {}

    def parse(self, *files):
        sys.argv[1:] = files
        self.ast = parse(language, fileinput.input(), skipWS=False)
        self.ast = Preparser(self.env).parse(self.ast)
        return self.ast

    def compile(self):
        self.compiler = Compiler(config=self.env)
        self.r.extend(self.compiler.compile(self.ast))
        return self.r

    def compose(self, where, declarations=False):
        self.compiler.check()
        for line in self.r:
            if line.startswith('VA') == declarations:
                where.write(line + '\n')

class pragma_dispatcher(overloaded):

    def key(self, *names):
        return names

    def key2(self, *args):
        return args[0]

    def search(self, key2):
        for names, meth in self._types.items():
            if type(names) is tuple and key2 in names:
                return meth

class PreparserError(Exception): pass

class Preparser(object):
    library = {
        '__builtin__': None,
    }

    def __init__(self, environ=None):
        self.depth = 0
        self.skip = [False]             # If at this level code is ignored
        self.matched = [False]          # If at this level an if (x) was true
        self.environ = environ or {}

    def eval(self, condition):
        return eval(condition, self.library, self.environ)

    def parse(self, ast):
        self.tree = []
        # Only parse top level of the AST
        for node in ast:
            self.visit(node)
        if self.depth > 0:
            raise PreparserError("Openended pragma")
        return self.tree

    def visit(self, node):
        if node.__name__ == 'pragma':
            if ' ' in node.what[0]:
                name, args = node.what[0].split(' ',1)
            else:
                name = node.what[0]
                args = ''
            self.handle(name, args, node)
        elif not self.skipping:
            # Only include this statement if all levels between the top #if
            # and here indiciate skip=False
            self.tree.append(node)

    @property
    def skipping(self):
        return reduce(operator.or_, self.skip)

    @pragma_dispatcher()
    def handle(self, keyword, args, node):
        raise PreparserError("{0}: Unsupported pragma: {1}".format(
            keyword, node.__name__.line))

    @handle.when('if')
    def handle(self, keyword, args, node):
        try:
            self.matched.append(self.eval(args))
        except:
            # Assume false since evaluation failed
            self.matched.append(False)
        self.skip.append(not self.matched[-1])
        self.depth += 1

    @handle.when('elseif')
    def handle(self, keyword, args, node):
        if self.depth == 0:
            raise PreparserError("{0}: 'elseif' without 'if'".format(
                node.__name__.line))
        # If no if or elseif has matched at this level, consider the
        # condition. If the condition is true, then don't skip the code in
        # this section.
        elif not self.matched[self.depth]:
            self.matched[self.depth] = self.eval(args)
            self.skip[self.depth] = not self.matched[self.depth]
        else:
            self.skip[self.depth] = True

    @handle.when('else')
    def handle(self, keyword, args, node):
        if self.depth == 0:
            raise PreparserError("{0}: 'else' without 'if'".format(
                node.__name__.line))
        # None of the other if statements matched. Include the #else
        # portion of the block
        self.skip[self.depth] = self.matched[self.depth]

    @handle.when('endif')
    def handle(self, keyword, args, node):
        if self.depth == 0:
            raise PreparserError("{0}: Unmached 'end if'".format(
                node.__name__.line))
        self.skip.pop()
        self.matched.pop()
        self.depth -= 1

    @handle.when('define')
    def handle(self, keyword, args, node):
        if self.skipping:
            return
        what = args.split(' ', 1)
        val = None
        if len(what) == 2:
            val = what.pop()
            try:
                val = self.eval(val)
            except ValueError as e:
                raise ValueError("#define {0}: {1}: {2}".format(
                    what, val, e.message))
        what = what[0]
        self.environ[what] = val

    @handle.when('include')
    def handle(self, keyword, args, node):
        if args in self.environ['modules'].split():
            return
        filename = literal_eval(args)
        self.environ['modules'] += ' ' + filename

        import os.path
        dir = os.path.dirname(node.__name__.line[1])
        filename = dir + '/' + filename

        self.tree.extend(Parser(self.environ).parse(filename))
