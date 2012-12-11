class overloaded(object):
    """
    Simple (well, simple is relative) tool to allow for overloaded methods
    in Python. Overloaded methods are methods with the same name that
    receive different types of arguments. Statically typed languages such as
    C++ support overloaded methods at compile time; however, dynamically
    typed languages such as Python cannot support such methods, because no
    type hints can be specified in the function definition.

    This class operates as a decorator and allows for a cascaded design of
    method definitions that are decorated with a calling signature. The
    initially decorated method will become the default method if no other
    overloaded method signatures match the argument types received in the
    function invocation. Subsequent method definitions are decorated with
    the "when" method of the default method and give the signature of the
    arguments to match.

    The overloaded methods are welcome to receive varying numbers of
    arguments. The overloaded default method decorator can receive an
    argument to indicate the number of arguments to match when searching for
    an overloaded method. The default is one.

    Subclasses of the declared argument types are also permitted; however,
    because method declarations are stored in a dictionary and searched in
    hash order, there is no guarantee which version of an overloaded method
    will be invoked if more than one match. If there is an exact match on
    the specific subtypes, though, that method is guaranteed to be invoked.

    >>> class A(object):
    ...     @overloaded(num_args=1)
    ...     def visit(self, what):
    ...         return "Unknown"
    ...
    ...     @visit.when(int)
    ...     def visit(self, what):
    ...         return "Integer"
    ...
    ...     @visit.when(float)
    ...     def visit(self, what):
    ...         return "Float"
    ...
    >>> A().visit(0)
    'Integer'
    >>> A().visit(1.1)
    'Float'

    Inheritance is possible and works well with the Python super() function.
    In the subclass definition, call the inherit() method of the overloaded
    method statically and then provide overloaded method definitions in the
    subclass:

    >>> class B(A):
    ...
    ...     visit = A.visit.inherit()
    ...
    ...     @visit.when(int)
    ...     def visit(self, what):
    ...         return "Super " + super(B, self).visit(what)
    ...
    >>> B().visit(0)
    'Super Integer'
    """
    def __init__(self, num_args=1, parent=None, memoize=False):
        self.default = None
        self.parent = parent
        self.num_args = num_args
        self.memoize = memoize
        self._types = {}

    def __call__(self, default):
        # Called by invocation of initial decorator (@overloaded)
        self.default = default
        return self

    def __get__(self, instance, owner):
        # Called for each invocation of the overloaded method
        if instance is None:
            return self

        def dispatch(*args, **kwargs):
            key2 = self.key2(*args, **kwargs)
            func = self.lookup(key2).__get__(instance, owner)
            # Bind the method to the caller
            if not hasattr(func, 'im_class'):
                func = func.__get__(instance, owner)
            return func(*args, **kwargs)
        return dispatch

    def lookup(self, key2):
        if key2 in self._types:
            return self._types[key2]
        else:
            if self.parent:
                meth = self.parent.lookup(key2)
            else:
                # Call generic visitor
                meth = self.search(key2) or self.default
        if self.memoize:
            self._types[key2] = meth
        return meth

    def inherit(self, num_args=None):
        # Called statically for subclasses with further overloaded methods
        return self.__class__(num_args or self.num_args, parent=self,
                memoize=self.memoize)

    def when(self, *args, **kwargs):
        # Main decorator for overloads
        def decorator(method):
            self._types[self.key(*args, **kwargs)] = method
            return self
        return decorator

    def key(self, *args, **kwargs):
        # Called at compile time to build the key signature for the method
        return tuple(args[:self.num_args])

    def key2(self, *args, **kwargs):
        # Called at runtime to convert the arguments passed to the method to
        # something that will match the signature created in ::key()
        return tuple(type(x) for x in args[:self.num_args])

    def search(self, key2):
        # Called at runtime when the value returned from key2() does not
        # match anything stored in the types hashtable. This method will be
        # used to see if the arguments are compatible with a signature in
        # the types hashtable -- for instance, it will consider subclasses
        for key, meth in self._types.items():
            for l,r in zip(key, key2):
                if not issubclass(r, l):
                    break
            else:
                return meth
