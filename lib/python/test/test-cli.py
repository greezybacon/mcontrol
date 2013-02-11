from __future__ import print_function

import sys, glob, os

install_path = os.path.abspath(
    os.path.dirname(os.path.abspath(__file__)) + '/..')

sys.path.insert(0, os.path.abspath(install_path))
for lib in glob.glob(install_path + '/build/lib.linux-*-{0}.{1}'.format(
        sys.version_info.major, sys.version_info.minor)):
    sys.path.insert(0, lib)

from cli import Shell, parser
options, args = parser.parse_args()
Shell(scripts=args, options=options).run()
