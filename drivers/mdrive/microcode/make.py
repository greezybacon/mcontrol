from __future__ import print_function

try:
    from configparser import SafeConfigParser
except:
    from ConfigParser import SafeConfigParser

from compile import parse

# Use python ast.eval_literal to interpret values from the config file
from ast import literal_eval
import warnings
import sys

def showwarning(message, category, filename, lineno, file=None, line=None):
    sys.stderr.write("{0}: {1}\n".format(category.__name__, message))
warnings.showwarning = showwarning

# Read in configuration
# XXX: Make this configurable
config = SafeConfigParser()
config.read('config.ini')

for axis in config.sections():
    # Compile configuration
    vars = {}
    for section in ('DEFAULT', axis):
        for k,v in config.items(section):
            try:
                vars[k] = literal_eval(v)
            except ValueError, e:
                raise ValueError("{0}: {1}: {2}: {3}".format(
                    section, k, v, e.message))

    print("Building code for {0}-axis".format(axis))

    p = parse.Parser(environ=vars)

    modules = vars['modules'].split()
    modules = ['modules/{0}'.format(x) for x in modules]

    p.parse(*modules)
    p.compile()

    p.compose(open('{0}-axis.mtx'.format(axis), 'wt'))
