Command-line Interface
======================
The cli is intended to provide comprehensive access to the features of
mcontrol as well as (within reason) details about connected motors and motor
drivers.

The cli is built on Python's cmd library and

Compatibility
-------------
The cli is tested to work with Python >= 2.6.0, including Python 3.x
versions.

Settings
--------
Settings for the cli are stored in the $HOME folder under ".mcontrol-cli/".
There are three settings files

### history
Python's cmd library uses readline to read commands from the user. The
readline history is automatically cached to the "history" file in the
settings folder at the conclusion of the cli session if not ended by an
exception.

### startup
Items placed in the startup script are automatically executed each time the
cli is invoked. Anything to make the cli environment fix the needs of a
particular system that is not addressable via a settings change can be coded
here. Note that incorrect syntax in this file will prevent using the cli
altogether.

### settings
The cli can be configured in the "settings" file. Refer to the default file
in `lib/python/cli/data/settings` for the latest list of supported settings.
Note that if you are adding new settings to the cli, please include
information about the setting's configuration in the default settings file.

#### Paths
One of the main points for the settings file is to indicate to the cli where
the included microcode, firmware, and motor drivers are located at on the
file system. This mainly help separate the locations from the RPM-installed
location and the location in the build tree used when testing.

Modules
-------
The cli is split into several modules, you can refer to each module's
respective documentation

  - [Connecting and context](cli-connect.md)
  - [Micro daemon mode](cli-standalone.md)
  - [Motor operation](cli-motor.md)
  - [Naming](cli-naming.md)
  - [Testing framework](cli-test.md)
  - [Tracing](cli-trace.md)
