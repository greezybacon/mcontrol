from distutils.core import setup, Extension

from Cython.Distutils import build_ext

import os
if os.name == 'posix':
    mcontrol_cli = "bin/mcontrol-cli"
else:
    mcontrol_cli = "bin/mcontrol-cli.py"

setup(
    name="mcontrol",
    version="0.1-3",
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
