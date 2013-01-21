from __future__ import print_function

import sys, glob, os

install_path = os.path.abspath(
    os.path.dirname(os.path.abspath(__file__)) + '/..')

sys.path.insert(0, os.path.abspath(install_path + '/cli'))
for lib in glob.glob(install_path + '/build/lib.linux-*-{0}.{1}'.format(
        sys.version_info.major, sys.version_info.minor)):
    sys.path.insert(0, lib)

import cli
if len(sys.argv) > 1:
    cli.Shell(script=sys.argv[1]).run()
else:
    cli.Shell().run()
