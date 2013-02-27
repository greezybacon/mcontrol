from . import Mixin, initializer, destructor

try:
    import configparser
except ImportError:
    # Python 2k
    import ConfigParser as configparser

import os.path
import readline

class Settings(object):
    """
    Nice settings interface that allows access to the ini-style settings in
    a dict-style interface
    """
    defaults = os.path.dirname(__file__) + '/../data/settings'

    def __init__(self, filename):
        self.conf = configparser.SafeConfigParser()
        if not os.path.exists(filename):
            self.write_defaults(filename)
        self.conf.read(filename)

    def write_defaults(self, where):
        with open(where, 'wt') as sf:
            with open(self.defaults, 'rt') as defaults:
                sf.write(defaults.read())
                sf.flush()

    def __getitem__(self, name):
        if ':' in name:
            section, name = name.split(':', 1)
            return self.get_setting(name, section)
        else:
            return self.get_setting(name)

    def __contains__(self, name):
        return self[name] is not None

    def get_setting(self, name, section='cli', default=None, type=None):
        try:
            setting = self.conf.get(section, name)
            if type:
                setting = type(setting)
            return setting
        except configparser.NoOptionError:
            pass
        return default

    def get_settings(self, section=None):
        if section:
            return dict(self.conf.items(section))
        else:
            settings = {}
            for s in self.sections:
                for k, v in self.conf.items(s):
                    settings['{0}:{1}'.format(s, k)] = v
            return settings

    @property
    def sections(self):
        return self.conf.sections()

class StartupMixin(Mixin):
    """
    Initializes the CLI cache folder and loads settings and history if any
    are defined.

    ~/.mcontrol-cli/        -- Cache folder
        /settings            + Settings file -- details follow
        /history             + History file
        /startup             + Commands to run at startup

    Settings supported
    [cli]
    history = -1                Length of history to keep
    history-file = history      History file
    standalone = 0              Enable standalone mode at startup

    [paths]
    microcode = /path/to/code   Default search path for microcode files
    firmware = /path/to/files   Default search path for firmware files
    drivers = /path/to/drivers  Default search path for mcontrol drivers

    [motors]
    prefix = driver://connecti  Default connection string prefix
    a = mdrive:///dev/ttyM...   Shortcut for 'connect a<Tab>'
    """
    @initializer
    def _create_cache_folder(self):
        self.cache_folder = os.path.expanduser('~/.mcontrol-cli')
        if not os.path.exists(self.cache_folder):
            os.mkdir(self.cache_folder)

    @initializer
    def _load_defaults(self):
        settings = os.path.join(self.cache_folder, 'settings')
        self['settings'] = Settings(settings)
        if self['settings']['cli:standalone']:
            self.do_standalone('')

    @initializer
    def _run_startup_script(self):
        startup = os.path.join(self.cache_folder, 'startup')
        if os.path.exists(startup):
            self.cmdqueue = open(startup, 'rt').readlines() + self.cmdqueue

    @initializer
    def initialize_history(self):
        self._history = self.get_setting('history-file', 'cli',
            default=os.path.join(self.cache_folder, 'history'))
        if os.path.exists(self._history):
            readline.read_history_file(self._history)
        readline.set_history_length(self.get_setting('history', 'cli',
            default=-1, type=int))

    @destructor
    def save_history(self):
        readline.write_history_file(self._history)

    def get_setting(self, name, section=None, default=None, type=None):
        return self['settings'].get_setting(name, section, default, type)

    def get_settings(self, section=None):
        return self['settings'].get_settings(section)

    def do_show(self, line):
        """
        Shows currently-configured settings. If called without an argument,
        all settings will be shown. Setting are stored in a file in your
        home folder, in ~/.mcontrol-cli/settings. Settings are loaded
        exactly once when the cli starts up

        Usage:
        show [setting]
        """
        if line and line not in self['settings']:
            return self.error("{0}: Setting not defined")
        elif line:
            self.out(self['settings'][line])
        else:
            # Calc max setting name length
            left = max(len(x) for x in self.get_settings().keys())
            for n, s in sorted(self.get_settings().items()):
                self.status("{0} => {1}".format(n.ljust(left), s))

    def complete_show(self, text, line, start, end):
        if ':' in line:
            section = line.split()[1].split(':')[0]
            return [x for x in self.get_settings(section).keys()
                if x.startswith(text)]
        else:
            return [x + ':' for x in self['settings'].sections
                if x.startswith(text)]
