from . import overloaded

from . import grammar

from .pyPEG import Symbol
import re


class ast_dispatcher(overloaded):

    def key(self, *names):
        return names

    def key2(self, node):
        return node.__name__

    def search(self, key2):
        for names, meth in self._types.items():
            if type(names) is tuple and key2 in names:
                return meth

class Name(object):

    def __init__(self, name, line=None):
        self.name = name
        self.line = line
        self.defined = False

    @classmethod
    def fromName(cls, name):
        i = cls(name.name)
        i.defined = name.defined
        i.line = name.line
        return i

    def update(self, other):
        assert other.name == self.name
        self.defined = self.defined or other.defined

    def __repr__(self):
        return self.__unicode__()
    def __unicode__(self):
        return "{0}: {1}: {2} @ {3}".format(
            type(self).__name__, self.name, self.defined, self.line)

class Label(Name):
    def __init__(self, name):
        super(Label, self).__init__(name)
        self.calls = 0      # CL <name>
        self.branches = 0   # BR <name>
        self.has_return = False

    def update(self, other):
        super(Label, self).update(other)
        self.calls += other.calls
        self.branches += other.branches
        self.has_return = self.has_return or other.has_return

    def __repr__(self):
        return self.__unicode__()
    def __unicode__(self):
        return "{0}: {1}: {2} {3}/{4}".format(
            type(self).__name__, self.name, self.defined, self.calls,
            self.branches)

class Variable(Name):
    def __init__(self, name):
        super(Variable, self).__init__(name)
        self.references = 0
        self.assignments = 0
        self.default = None

    def update(self, other):
        super(Variable, self).update(other)
        self.references += other.references
        self.assignments += other.assignments
        self.default = self.default or other.default

class CompileError(Exception): pass

class Compiler(object):
    checks = []

    def __init__(self, config=None):
        self.setup_stack()
        self.config = config or {}

    def setup_stack(self):
        self.stack = [[]]
        self.label = None
        self.names = {}
        self.depth = 0
        self.current_frame = self.stack[-1]
        self.defaults = []
        self.current_label = None

    def PUSH(self, what):
        self.current_frame.append(what)

    def POP(self):
        return self.current_frame.pop()

    def PEEK(self):
        return self.current_frame[-1]

    def PUSH_BLOCK(self):
        self.stack.append([])
        self.current_frame = self.stack[-1]
        self.depth += 1

    def POP_BLOCK(self):
        self.current_frame = self.stack[-2]
        self.depth -= 1
        return self.stack.pop()

    @classmethod
    def add_check(cls, what):
        cls.checks.append(what)

    def merge(self, compiler):
        for name in compiler.names.values():
            if name.name in self.names:
                self.names[name.name].update(name)
            else:
                self.names[name.name] = name

    def compile(self, ast):
        for node in ast:
            self.visit(node)
        if self.depth > 0:
            raise CompileError("Openended pragma")
        return self.stack[0]

    def check(self):
        for check in self.checks:
            check(self)

    def parse_var(self, var):
        """
        Parses a configvar node and returns the value from the local
        environment. For instance, $var will result in self.config["var"],
        and $var.item will result in self.config["var"]["item"]
        """
        val = None
        try:
            for name in var.split('.'):
                if val is None:
                    val = self.config[name]
                else:
                    val = val[name]
        except KeyError:
            warn("{0}: Undefined config variable".format(var), Warning)
            val = "${0}".format(var)
        return val

    @ast_dispatcher(memoize=True)
    def visit(self, node):
        for w in node.what:
            if isinstance(w, Symbol):
                self.visit(w)

    @visit.when('comment')
    def visit(self, node):
        pass

    @visit.when('statement')
    def visit(self, node):
        self.PUSH_BLOCK()
        for w in node.what:
            self.visit(w)
        self.PUSH(' '.join(self.POP_BLOCK()))

    @visit.when('assignment')
    def visit(self, node):
        self.PUSH_BLOCK()
        for w in node.what:
            self.visit(w)
        items = self.POP_BLOCK()
        name = items.pop(0)
        if name in grammar.label_args:
            # This command receives a label (not a variable) as an argument
            for i in items:
                try:
                    l = self.FIND_LABEL(i, force_convert=True)
                    l.calls += 1
                except KeyError:
                    # Not a label
                    pass
        lhs = self.FIND_VAR(name)
        lhs.assignments += 1
        if name in grammar.read_only:
            raise CompileError("Assignment to read-only internal variable {0}"
                .format(name))
        elif name in grammar.internal and name not in grammar.assignable:
            raise CompileError("{0}: Assignment to internal command"
                .format(name))

        # TODO: Keep track of defaults / assignments within labeled blocks.
        #       Defaults should be emitted separately, and commonly, before
        #       the first label is defined
        #if self.current_label is None:
        #    self.defaults.append(lhs)
        #    lhs.default = ", ".join(items)
        operator = '= ' if name not in grammar.commands else ''
        self.PUSH("{0} {1}{2}".format(name, operator, ", ".join(items)))

    @visit.when('declaration')
    def visit(self, node):
        self.PUSH_BLOCK()
        for w in node.what:
            self.visit(w)
        B = self.POP_BLOCK()
        name = self.FIND_VAR(B[0])
        name.defined = True
        if len(B) == 2:
            self.PUSH("VA {0} = {1}".format(*B))
            name.default = B[1]
        else:
            self.PUSH("VA {0}".format(*B))

    @visit.when('number', 'compare', 'math', 'command', 'unary')
    def visit(self, node):
        self.PUSH(node.what)

    @visit.when('string')
    def visit(self, node):
        s = node.what
        for m in re.finditer(r"\$([\w_.]+)", node.what):
            s = s[:m.start()] + self.parse_var(m.group(1)) + s[m.end():]
        self.PUSH(s)

    @visit.when('expression')
    def visit(self, node):
        self.PUSH_BLOCK()
        for w in node.what:
            self.visit(w)
        B = self.POP_BLOCK()
        try:
            self.FIND_VAR(B[0]).references += 1
        except KeyError:
            # Undeclared variable -- warning will be issued later if not
            # defined before the end of the microcode
            pass
        except CompileError:
            # If variable is used as a label, or visa-versa, emit the actual
            # error somewhere else
            pass

        expr = ' '.join(B)
        # Attempt to reduce the expression
        # XXX: This is problematic as it is evaluted right-to-left instead
        #      of the usual left-to-right. Furthermore, it will result in
        #      loss of precision (X * 7 / 10, would be reduced to X * 0)
        #try:
        #    expr = repr(eval(expr, {'__builtins__':None}, {}))
        #except:
        #    pass
        self.PUSH(expr)

    @visit.when('label')
    def visit(self, node):
        self.visit(node.what[0])
        self.current_label = l = self.FIND_LABEL(node.what[0].what)
        self.label = l.name
        if l.defined:
            raise CompileError("Label '{0}' redefined, from {1}"
                .format(l.name, node.__name__.line))
        elif l.name in grammar.internal:
            raise CompileError(
                "{0}: Internal variable used as label, from {1}"
                .format(l.name, node.__name__.line))
        else:
            l.defined = True
            l.where = node.__name__.line

        self.PUSH("LB {0}".format(self.POP()))

    @visit.when('name')
    def visit(self, node):
        # Keep track of defined/referenced names
        if not node.what in self.names:
            self.names[node.what] = Name(node.what, node.__name__.line)
        self.PUSH(node.what)

    def FIND_LABEL(self, name, force_convert=False):
        name = self.names[name]
        if type(name) is Name or force_convert:
            self.names[name.name] = name = Label.fromName(name)
        elif type(name) is not Label:
            raise CompileError('{0} used as a label'.format(
                name))
        return name

    def FIND_VAR(self, name):
        name = self.names[name]
        if type(name) is Name:
            self.names[name.name] = name = Variable.fromName(name)
        elif type(name) is not Variable:
            raise CompileError('{0} used as a variable, from {1}'.format(
                name.name, name.line))
        return name

    @visit.when('call', 'branch')
    def visit(self, node):
        self.PUSH_BLOCK()
        for w in node.what:
            self.visit(w)
        B = self.POP_BLOCK()

        l = self.FIND_LABEL(node.what[0].what)
        if node.__name__ == 'call':
            l.calls += 1
            self.PUSH('CL {0}'.format(B.pop(0)))
        elif node.__name__ == 'branch':
            l.branches += 1
            self.PUSH('BR {0}'.format(B.pop(0)))

        if len(B):
            self.PUSH("{0}, {1}".format(self.POP(), B.pop(0)))

    @visit.when('return_')
    def visit(self, node):
        l = self.FIND_LABEL(self.label)
        l.has_return = True
        self.PUSH('RT')

    @visit.when('configvar')
    def visit(self, node):
        self.PUSH(repr(self.parse_var(node.what[0])))

from warnings import warn

@Compiler.add_check
def unused_variables(compiler):
    for name in compiler.names.values():
        if type(name) is not Variable:
            continue
        elif name.name in grammar.internal:
            continue

        if name.references + name.assignments == 0:
            warn("{0}: {1}: Unused variable".format(name.line, name.name), Warning)
        if name.defined == False:
            warn("{0}: {1}: Undeclared variable".format(name.line, name.name), Warning)

@Compiler.add_check
def undefined_labels(compiler):
    for name in compiler.names.values():
        if type(name) is not Label:
            continue

        if name.branches + name.calls == 0:
            warn("{0}: {1}: Unused label".format(name.line, name.name), Warning)
        if name.defined == False:
            warn("{0}: {1}: Undeclared label".format(name.line, name.name), Warning)
            #raise CompileError("{0}: {1}: Reference to undeclared label"
            #    .format(name.line, name.name))
