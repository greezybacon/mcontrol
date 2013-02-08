from . import Mixin, initializer, destructor

try:
    import configparser
except ImportError:
    # Python 2k
    import ConfigParser as configparser

import os.path
import readline

class StartupMixin(Mixin):
    """
    Initializes the CLI cache folder and loads settings and history if any
    are defined.

    ~/.mcontrol-cli/        -- Cache folder
        /settings            + Settings file -- details follow
        /history             + History file
        /startup             + Commands to run at startup

    Settings supported
    [global]                    Global section header is not required
    history = -1                Length of history to keep
    history-file = history      History file

    [paths]
    microcode = /path/to/code   Default search path for microcode files
    firmware = /path/to/files   Default search path for firmware files
    """
    @initializer
    def _create_cache_folder(self):
        self.cache_folder = os.path.expanduser('~/.mcontrol-cli')
        if not os.path.exists(self.cache_folder):
            os.mkdir(self.cache_folder)

    @initializer
    def _load_defaults(self):
        settings = os.path.join(self.cache_folder, 'settings')
        self['settings'] = configparser.SafeConfigParser()
        # Write default settings to the settings file
        if not os.path.exists(settings):
            with open(settings, 'wt') as sf:
                with open(os.path.dirname(__file__)
                        + '/../data/settings', 'rt') as defaults:
                    sf.write(defaults.read())
                    sf.flush()
        self['settings'].read(settings)
        if self.get_setting('standalone', 'cli', default=False):
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

    def get_setting(self, name, section='DEFAULT', default=None, type=None):
        try:
            if 'settings' in self.context:
                setting = self['settings'].get(section, name)
                if type:
                    setting = type(setting)
                return setting
        except configparser.NoOptionError:
            pass
        return default

    def get_settings(self, section='DEFAULT'):
        if 'settings' in self.context:
            return dict(self['settings'].items(section))
        return {}
