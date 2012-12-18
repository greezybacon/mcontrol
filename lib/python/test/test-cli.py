from __future__ import print_function

import sys, glob
sys.path.insert(0, '../cli')
for lib in glob.glob('../build/lib.linux-*-{0}.{1}'.format(
        sys.version_info.major, sys.version_info.minor)):
    sys.path.insert(0, lib)

import cli
cli.Shell().run()
