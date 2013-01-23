cimport mcontrol.constants as k
from cpython cimport bool
from mcontrol cimport defs

all_units = {
    k.MILLI_INCH:   "mil",
    k.INCH:         "inch",
    k.MILLI_METER:  "mm",
    k.METER:        "m",
    k.MILLI_G:      "milli-g",
    k.MILLI_DEGREE: "milli-deg",
    k.MILLI_ROTATION: "milli-rev",
    k.MICRO_REVS:   "micro-rev",    # Raw units

    "inch":         k.INCH,
    "mil":          k.MILLI_INCH,
    "m":            k.METER,
    "mm":           k.MILLI_METER,
    "milli-g":      k.MILLI_G,
    "milli-deg":    k.MILLI_DEGREE,
    "milli-rev":    k.MILLI_ROTATION,
    "micro-rev":    k.MICRO_REVS,   # Raw units
}

abbreviations = {
    'in':           k.INCH
}

conversions = {
    k.INCH:         (1000, k.MILLI_INCH),
    k.METER:        (1000, k.MILLI_METER),
    'g':            (1000, k.MILLI_G),

    # Raw measure of motor spindle rate
    'rpm':          (1000000 / 60., k.MICRO_REVS),
}

def scale_up(value, units):
    if units in (k.INCH, 'inch'):
        return value * 1000, 'mil'
    elif units in (k.METER, 'm'):
        return value * 1000, 'mm'
    return value, units

class NoDaemonException(Exception): pass
class CommFailure(Exception): pass

cdef extern from "lib/client.h" nogil:
    enum mcCallMode:
        MC_CALL_IN_PROCESS
        MC_CALL_CROSS_PROCESS

cdef class Library:
    @classmethod
    def run_in_process(cls):
        # Set call mode to in-process (not out-of-process)
        mcClientCallModeSet(MC_CALL_IN_PROCESS)

    @classmethod
    def run_out_of_process(cls):
        mcClientCallModeSet(MC_CALL_CROSS_PROCESS)

    @classmethod
    def is_in_process(cls):
        return mcClientCallModeGet() == MC_CALL_IN_PROCESS

    @classmethod
    def load_driver(cls, name):
        import os
        if not os.path.exists(name):
            raise ValueError("{0}: Driver library does not exist"
                .format(name))
        name = name.encode('latin-1')
        cdef char * cstring = <char *>name
        with nogil:
            mcDriverLoad(cstring)

cdef class Motor:

    cdef readonly int id
    cdef int _units
    cdef long long _scale
    cdef bool _has_profile
    cdef MotorProfile _profile

    def __init__(self, connection, recovery=False):
        cdef int status
        cdef String buf = bufferFromString(connection)
        cdef int _recovery = recovery
        with nogil:
            status = mcConnect(&buf, &self.id, _recovery)

        raise_status(status, "Unable to connect to motor")

    def disconnect(self):
        """
        Ordinarily, connections to the motor's device are held by the daemon
        until its exit. This will preserve statistics and synced motor
        characteristics (motion profile, communication, etc) to speed future
        connections to the same motor. This method can be used to instruct
        the connection to the motor to be severed. This is mostly useful if
        the motor is about to be replaced by another or the motor is about
        to be used by different software without closing the mcontrol daemon
        """
        with nogil:
            mcDisconnect(self.id)

    def _get_value_and_units(self, value, units=None):
        """
        Allows the user to input units with a command, and the system will
        convert them to the unit numbers used by mcontrol. Also, if a
        floating point number is given, the value will be converted to a
        smaller unit automagically
        """
        units = units or self._units

        # Unabbreviate and convert the units
        if units in abbreviations:
            units = abbreviations[units]
        if units in conversions:
            conv = conversions[units]
            value = float(value) * conv[0]
            units = conv[1]
        else:
            value = int(value)
        if type(units) is not int:
            if units not in all_units:
                raise ValueError("Unknown units given")
            units = all_units[units]

        return value, units

    property units:
        def __get__(self):
            return self._units

    property scale:
        def __get__(self):
            return self._scale

        def __set__(self, urevs):
            cdef int status

            if type(urevs) not in (list, tuple) or len(urevs) != 2:
                raise ValueError("Two-item tuple expected")
            scale, units = urevs
            if type(units) is not int:
                if units not in all_units:
                    raise ValueError("Unsupported units")
                units = all_units[units]

            cdef int _units = units, _scale = scale
            with nogil:
                status = mcUnitScaleSet(self.id, _units, _scale)
            raise_status(status, "Unable to configure scale and untis")
            self._scale = scale
            self._units = units

    property position:
        def __get__(self):
            cdef double position
            cdef int status
            with nogil:
                status = mcQueryFloat(self.id, k.MCPOSITION, &position)
            raise_status(status, "Unable to retrieve current position")

            return position

        def __set__(self, position):
            cdef int status
            units = None
            if type(position) is tuple:
                position, units = self._get_value_and_units(*position)

            cdef int _position = position, _units = units
            if units:
                with nogil:
                    status = mcPokeIntegerUnits(self.id, k.MCPOSITION,
                        _position, _units)
            else:
                with nogil:
                    status = mcPokeInteger(self.id, k.MCPOSITION, _position)

            raise_status(status, "Unable to set device position")

    property velocity:
        def __get__(self):
            cdef double velocity
            cdef int status
            with nogil:
                status = mcQueryFloat(self.id, k.MCVELOCITY, &velocity)
            raise_status(status, "Unable to retrieve current velocity")
            return velocity

    property moving:
        def __get__(self):
            cdef int status, moving
            with nogil:
                status = mcQueryInteger(self.id, k.MCMOVING, &moving)
            raise_status(status, "Unable to query device status")
            return not not moving

    property stalled:
        def __get__(self):
            cdef int status, stalled
            with nogil:
                status = mcQueryInteger(self.id, k.MCSTALLED, &stalled)
            raise_status(status, "Unable to retrieve device state")
            return not not stalled

        def __set__(self, state):
            cdef int status, value
            value = 1 if state else 0
            with nogil:
                status = mcPokeInteger(self.id, k.MCSTALLED, value)
            raise_status(status, "Unable to set stall flag")

    property profile:
        def __get__(self):
            if not self._has_profile:
                self._profile = MotorProfile(self)
                self._has_profile = True
            return self._profile

        def __set__(self, profile):
            if not isinstance(profile, MotorProfile):
                raise ValueError("MotorProfile instance expected")
            if not profile.motor is self:
                profile = profile.copy()
                profile.motor = self
            self._profile = profile
            # Send the profile to the motor
            self._profile.commit()

    def move(self, how_much, units=None):
        cdef int status, _how_much = how_much
        cdef int _units = units or self.units
        with nogil:
            status = mcMoveRelativeUnits(self.id, _how_much, _units)
        raise_status(status, "Unable to command relative move")

    def moveTo(self, where, units=None):
        cdef int status, _where = where, _units = units or self.units
        with nogil:
            status = mcMoveAbsoluteUnits(self.id, _where, _units)
        raise_status(status, "Unable to command absolute move")

    def slew(self, rate, units=None):
        cdef int status, _rate = rate, _units = units or self.units
        with nogil:
            status = mcSlewUnits(self.id, _rate, _units)
        raise_status(status, "Unable to slew motor")

    def stop(self):
        cdef int status
        with nogil:
            status = mcStop(self.id)
        raise_status(status, "Unable to stop motor")

    def halt(self):
        cdef int status
        with nogil:
            status = mcHalt(self.id)
        raise_status(status, "Unable to halt motor")

    def on(self, event):
        return Event(self, event)

    def load_firmware(self, filename):
        cdef String buf = bufferFromString(filename)
        cdef int status
        with nogil:
            status = mcFirmwareLoad(self.id, &buf);
        raise_status(status, "Unable to flash firmware")

    def load_microcode(self, filename):
        cdef String buf = bufferFromString(filename)
        cdef int status
        with nogil:
            status = mcMicrocodeLoad(self.id, &buf);
        raise_status(status, "Unable to load microcode")

    def search(cls, driver):
        cdef String dri = bufferFromString(driver)
        cdef String results
        cdef int count
        with nogil:
            count = mcSearch(0, &dri, &results)
        print("Found %d motors" % count)

    search = classmethod(search)

cdef extern from "drivers/mdrive/mdrive.h":
    ctypedef enum mdrive_read_variable:
        MDRIVE_SERIAL,
        MDRIVE_PART,
        MDRIVE_FIRMWARE,
        MDRIVE_MICROCODE,

        MDRIVE_BAUDRATE,
        MDRIVE_CHECKSUM,
        MDRIVE_ECHO,
        MDRIVE_ADDRESS,
        MDRIVE_NAME,
        MDRIVE_RESET,
        MDRIVE_HARD_RESET,

        MDRIVE_STATS_RX,
        MDRIVE_STATS_TX,

        MDRIVE_IO_TYPE,
        MDRIVE_IO_PARM1,
        MDRIVE_IO_PARM2,

        MDRIVE_ENCODER,
        MDRIVE_VARIABLE,
        MDRIVE_EXECUTE

    ctypedef enum mdrive_io_type:
        IO_INPUT,
        IO_HOME,
        IO_PAUSE,

        IO_OUTPUT,
        IO_MOVING

cdef class MdriveMotor(Motor):

    property address:
        def __get__(self):
            cdef String buf
            cdef int status
            with nogil:
                status = mcQueryString(self.id, MDRIVE_ADDRESS, &buf)
            if status != 0:
                raise RuntimeError(status)
            return buf.buffer.decode('latin-1')

        def __set__(self, address):
            cdef String buf = bufferFromString(address[0])
            cdef int status
            cdef timespec old
            cdef timespec new
            new.tv_sec = 5

            # This will take a while. Use a different timeout
            mcClientTimeoutSet(&new, &old)
            with nogil:
                status = mcPokeString(self.id, MDRIVE_ADDRESS, &buf)
            mcClientTimeoutSet(&old, NULL)

            if status != 0:
                raise RuntimeError(status)

    property baudrate:
        def __get__(self):
            cdef int val, status
            with nogil:
                status = mcQueryInteger(self.id, MDRIVE_BAUDRATE, &val)

            if status != 0:
                raise RuntimeError(status)
            else:
                return val

        def __set__(self, rate):
            cdef int status, _rate = rate
            with nogil:
                status = mcPokeInteger(self.id, MDRIVE_BAUDRATE, _rate)

            if status != 0:
                raise RuntimeError(status)

    property serial:
       def __get__(self):
            cdef String buf
            cdef int status
            with nogil:
                status = mcQueryString(self.id, MDRIVE_SERIAL, &buf)
            raise_status(status, "Unable to fetch serial number")
            return buf.buffer.decode('latin-1')

    property part:
        def __get__(self):
            cdef String buf
            cdef int status
            with nogil:
                status = mcQueryString(self.id, MDRIVE_PART, &buf)
            raise_status(status, "Unable to fetch part number")
            return buf.buffer.decode('latin-1')

    property firmware:
        def __get__(self):
            cdef String buf
            cdef int status
            with nogil:
                status = mcQueryString(self.id, MDRIVE_FIRMWARE, &buf)
            raise_status(status, "Unable to fetch firmware version")
            return buf.buffer.decode('latin-1')

    property encoder:
        def __get__(self):
            cdef int val, status
            with nogil:
                status = mcQueryInteger(self.id, MDRIVE_ENCODER, &val)
            raise_status(status, "Unable to fetch device encoder settings")
            return not not val

        def __set__(self, val):
            cdef int status, _val = val
            with nogil:
                status = mcPokeInteger(self.id, MDRIVE_ENCODER, _val)
            raise_status(status, "Unable to set device encoder setting")

    def factory_default(self):
        cdef int status
        with nogil:
            status = mcPokeInteger(self.id, MDRIVE_HARD_RESET, 1)
        raise_status(status, "Unable to factory default motor(s)")

    def read_port(self, port):
        cdef int val, status, _port = port
        with nogil:
            status = mcQueryIntegerWithIntegerItem(self.id, k.MCINPUT,
                &val, _port)
        raise_status(status, "Unable to read device port state")
        return val

    def write_port(self, port, value):
        cdef int status, _value = value, _port = port
        with nogil:
            status = mcPokeIntegerWithIntegerItem(self.id, k.MCOUTPUT, _value,
                _port)
        raise_status(status, "Unable to write device port state")

    def configure_port(self, port, **settings):
        cdef int status, _port = port, _setting
        if 'source' in settings:
            _setting = settings['source']
            with nogil:
                status = mcPokeIntegerWithIntegerItem(self.id, MDRIVE_IO_PARM2,
                    _setting, _port)
            raise_status(status, "Unable to configure port source/sink setting")
        if 'active_high' in settings:
            _setting = settings['active_high']
            with nogil:
                status = mcPokeIntegerWithIntegerItem(self.id, MDRIVE_IO_PARM1,
                    _setting, _port)
            raise_status(status, "Unable to configure port active-high/low setting")
        if 'type' in settings:
            types = {
                'input':        IO_INPUT,
                'output':       IO_OUTPUT,
                'home':         IO_HOME,
                'pause':        IO_PAUSE,

                # Output types
                'moving':       IO_MOVING,
            }
            if settings['type'].lower() not in types:
                raise ValueError("{0}: Unsupported port type"
                    .format(settings['type']))
            _setting = types[settings['type'].lower()]
            with nogil:
                status = mcPokeIntegerWithIntegerItem(self.id, MDRIVE_IO_TYPE,
                    _setting, _port)
            raise_status(status, "Unable to configure port type")

    def name_set(self, address, serial_number):
        cdef String addr = bufferFromString(address[0])
        cdef String sn = bufferFromString(serial_number)
        cdef int status

        with nogil:
            status = mcPokeStringWithStringItem(self.id, MDRIVE_NAME,
                &addr, &sn)
        raise_status(status, "Unable to name motor")

    def search(cls):
        return Motor.search('mdrive')
    search = classmethod(search)

    def call(self, label):
        cdef String _label = bufferFromString(label)
        cdef int status
        with nogil:
            status = mcPokeString(self.id, MDRIVE_EXECUTE, &_label)

    def read(self, variable):
        cdef int value, status
        cdef String var = bufferFromString(variable)
        with nogil:
            status = mcQueryIntegerWithStringItem(self.id, MDRIVE_VARIABLE,
                &value, &var)
        return value

    def write(self, variable, value):
        cdef String var = bufferFromString(variable)
        cdef int status, _value = value
        with nogil:
            status = mcPokeIntegerWithStringItem(self.id, MDRIVE_VARIABLE,
                _value, &var)

cdef void pyEventCallback(event_info_t * info) with gil:
    PyEval_InitThreads()
    assert info is not NULL
    assert info.user is not NULL
    cdef object data = None
    if info.event == defs.EV_MOTION:
        data = mdFromEventData(info.data)
    Event.callback(<object>info.user, data)

cdef class MotionDetails(object):

    cdef public bool completed
    cdef public bool stalled
    cdef public bool cancelled
    cdef public bool stopped
    cdef public bool in_progress
    cdef public bool pos_known
    cdef public int error
    cdef public object position

    def __repr__(self):
        return self.__unicode__()
    def __unicode__(self):
        state = pos = ""
        if self.completed:
            state = "ARRIVE"
        elif self.stalled:
            state = "STALL"
        elif self.cancelled:
            state = "CANCEL"
        elif self.stopped:
            state = "STOP"
        if self.pos_known:
            pos = " POS: %d".format(self.position)
        return "{0}{1}".format(state, pos)

cdef MotionDetails mdFromEventData(event_data_t * data):
    if data == NULL:
        return None

    cdef MotionDetails self = MotionDetails()
    self.completed = not not data.motion.completed
    self.stalled = not not data.motion.stalled
    self.cancelled = not not data.motion.cancelled
    self.stopped = not not data.motion.stopped
    self.in_progress = not not data.motion.in_progress
    self.error = not not data.motion.error
    if data.motion.pos_known:
        self.position = data.motion.position
    else:
        self.position = None
    return self

cdef class Event(object):
    EV_MOTION = defs.EV_MOTION

    cdef readonly Motor motor
    cdef readonly int event
    cdef readonly int id
    cdef readonly bool isset
    cdef object _callback
    cdef public object data

    def __init__(self, motor, event):
        self.motor = motor
        self.event = event
        self._callback = None
        self.id = 0
        self.reset()

    def __dealloc__(self):
        if self.id:
            mcUnsubscribe(self.motor.id, self.id)

    def reset(self):
        self.isset = False
        self.data = None
        if self.id:
            mcUnsubscribe(self.motor.id, self.id)
        raise_status(
            mcSubscribeWithData(
                self.motor.id, self.event, &self.id,
                pyEventCallback, <void *>self),
            "Unable to subscribe to event")

    cdef void callback(Event self, object data):
        self.isset = True
        self.data = data
        if self._callback:
            self._callback(self)

    def call(self, callback):
        self._callback = callback
        return self

    def wait(self):
        with nogil:
            mcEventWait(self.motor.id, self.event)
