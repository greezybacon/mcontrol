MControl is a comprehensive motor control suite intended to abstract away
the specifics about controlling motors. This is in hopes of utilizing the
same software to control various types of motors

Requirements
============

Cython
------
Building will require the Cython package to be installed. This can be
installed with yum

    yum install Cython

**However**, the package available to Fedora 11 is too old to build the
Cython modules included. Building on Fedora 11 will require a manual
installation of the Cython software. Refer to the [Cython
docs](http://docs.cython.org/) for more accurate and up-to-date downloading
and building instructions. You could likely use a procedure something like
this to build and install it on Fedora 11:

    wget http://cython.org/release/Cython-0.18.tar.gz
    tar xvzf Cython-0.18.tar.gz
    cd Cython-0.18
    python setup.py build
    sudo python setup.py install
    cd ..
    rm -rf Cython-0.18.tar.gz Cython-0.18

Platform
--------
There is no configure script to inspect the platform and environment.
Currently, Linux with glibc is the only supported build platform, and a
RedHat RPM is the only supported build target for package building.

Building
========
To build the software in-place, for testing, just make.

    make

A CentOS / RedHat / Fedora package can be built with

    make rpm

**Note** that when building RPMs, the current source tree according to `git`
is used for building. Therefore, if you make changes to the source tree, you
must commit them into git before they will be incorporated into the RPM
built. If you need experimental changes to be tested via an RPM, do
something like

    git checkout -b brach-i-dont-care-about
    git add ...
    git commit
    make rpm

Then, after sufficient testing, either merge or discard the branch with your
changes

    git checkout develop
    git merge branch-i-dont-care-about
    git branch -D brach-i-dont-care-about

**Warning** it seems that parallel building may fail the microcode building.
Use the `-j1` option if you have compile errors when building microcode.

Testing
=======
After building, just run the cli. It defaults to run in standalone mode

    LD_LIBRARY_PATH="lib" python lib/python/test/test-cli.py

Documentation
=============
Refer to the included [documentation](doc/index.md) for further reading.
