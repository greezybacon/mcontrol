from distutils.core import setup, Extension

from Cython.Distutils import build_ext

setup(
    name="mcontrol",
    version="0.1",
    packages=["mcontrol"],
    ext_modules = [
        Extension("_motor", [
                "mcontrol/mcontrol.pyx"
            ],
            include_dirs=["../../"],
            libraries=["mcontrol"], library_dirs=[".."])
    ],
    cmdclass = {'build_ext': build_ext}
)
