from __future__ import with_statement

from distutils.core import setup, Extension

from Cython.Distutils import build_ext

import os

if os.name == 'posix':
    mcontrol_cli = "bin/mcontrol-cli"
else:
    mcontrol_cli = "bin/mcontrol-cli.py"

if 'GIT_VERSION' in os.environ:
    version = os.environ['GIT_VERSION']
    with open(os.path.dirname(os.path.abspath(__file__)) + '/cli/version.py',
            'wt') as vfile:
        vfile.write('version="{0}"'.format(version))
else:
    version = 'development'

setup(
    name="mcontrol",
    version=version,
    packages=["mcontrol", "mcontrol.cli", "mcontrol.cli.modules",
        "mcontrol.cli.lib"],
    package_dir={
        'mcontrol': "mcontrol",
        'mcontrol.cli': "cli",
        'mcontrol.cli.modules': "cli/modules",
        'mcontrol.cli.lib': "cli/lib",
    },
    package_data={
        'mcontrol.cli': ['data/*'],
    },
    ext_modules = [
        Extension("_motor", [
                "mcontrol/mcontrol.pyx"
            ],
            include_dirs=["../../"],
            libraries=["mcontrol"], library_dirs=[".."])
    ],
    cmdclass = {'build_ext': build_ext},
    scripts = [mcontrol_cli]
)
