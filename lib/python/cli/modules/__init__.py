__all__ = ['connect', 'naming', 'microcode', 'startup', 'standalone',
           'test', 'trace', 'utils', 'Mixin']

class Mixin(object): pass

def initializer(func):
    """
    Specifies an initialization function to be run when the shell is
    constructed.
    """
    func.initializer = True
    return func

initializer.ignore = True

def destructor(func):
    """
    Specifies a function that should be run when the main CLI environment is
    shut down -- that is, when the CLI is exiting back to the operating
    system
    """
    func.destructor = True
    return func

destructor.ignore = True

def trim(docstring):
    if not docstring:
        return ''
    # Convert tabs to spaces (following the normal Python rules)
    # and split into a list of lines:
    lines = docstring.expandtabs().splitlines()
    # Determine minimum indentation (first line doesn't count):
    indent = 1000
    for line in lines[1:]:
        stripped = line.lstrip()
        if stripped:
            indent = min(indent, len(line) - len(stripped))
    # Remove indentation (first line is special):
    trimmed = [lines[0].strip()]
    if indent < 1000:
        for line in lines[1:]:
            trimmed.append(line[indent:].rstrip())
    # Strip off trailing and leading blank lines:
    while trimmed and not trimmed[-1]:
        trimmed.pop()
    while trimmed and not trimmed[0]:
        trimmed.pop(0)
    # Return a single string:
    return '\n'.join(trimmed)
