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

    "inch":         k.INCH,
    "mil":          k.MILLI_INCH,
    "m":            k.METER,
    "mm":           k.MILLI_METER,
    "milli-g":      k.MILLI_G,
    "milli-deg":    k.MILLI_DEGREE,
    "milli-rev":    k.MILLI_ROTATION
}

def scale_up(value, units):
    if units in (k.INCH, 'inch'):
        return value * 1000, 'mil'
    elif units in (k.METER, 'm'):
        return value * 1000, 'mm'
    return value, units

class NoDaemonException(Exception): pass
class CommFailure(Exception): pass

cdef class Motor:

    cdef readonly int id
    cdef int _units
    cdef long long _scale
    cdef bool _has_profile
    cdef MotorProfile _profile

    def __init__(self, connection, recovery=False):
        cdef int status
        cdef String buf = bufferFromString(connection)
        status = mcConnect(&buf, &self.id)

        raise_status(status, "Unable to connect to motor")

    def _fetch_units_and_scale(self):
        cdef int status
        status = mcUnitScaleGet(self.id, &self._units, &self._scale)

        if status != 0:
            raise RuntimeError(status)

    property units:
        def __get__(self):
            if self._units == 0:
                self._fetch_units_and_scale()
            return self._units

        def __set__(self, units):
            if self._scale == 0:
                self._fetch_units_and_scale()
            cdef int status
            status = mcUnitScaleSet(self.id, units, self.scale)

            if status != 0:
                raise RuntimeError(status)
            else:
                self._units = units

    property scale:
        def __get__(self):
            self._fetch_units_and_scale()
            return self._scale

        def __set__(self, urevs):
            self._fetch_units_and_scale()
            cdef int status
            status = mcUnitScaleSet(self.id, self._units, urevs)

            if status != 0:
                raise RuntimeError(status)
            else:
                self._scale = urevs

    property position:
        def __get__(self):
            cdef int status, position
            status = mcQueryInteger(self.id, k.MCPOSITION, &position)

            if status != 0:
                raise RuntimeError(status)

            return position

        def __set__(self, position):
            units = None
            if type(position) is tuple:
                if len(position) != 2:
                    raise ValueError("Two item tuple required: "
                        "(val, units)")
                position, units = position

                if type(units) not in (int, type(None)):
                    if units not in all_units:
                        raise ValueError("Unsupported units")
                    units = all_units[units]

            if units:
                raise_status(
                    mcPokeIntegerUnits(self.id, k.MCPOSITION, position,
                        units),
                    "Unable to set device position")
            else:
                raise_status(
                    mcPokeInteger(self.id, k.MCPOSITION, position),
                    "Unable to set device position")

    property moving:
        def __get__(self):
            cdef int status, moving
            status = mcQueryInteger(self.id, k.MCMOVING, &moving)

            if status != 0:
                raise RuntimeError(status)

            return not not moving

    property stalled:
        def __get__(self):
            cdef int stalled
            raise_status(
                mcQueryInteger(self.id, k.MCSTALLED, &stalled),
                "Unable to retrieve device state")
            return not not stalled

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
        raise_status(
            mcMoveRelativeUnits(self.id, how_much, units or self.units),
            "Unable to command relative move")

    def moveTo(self, where, units=None):
        raise_status(
            mcMoveAbsoluteUnits(self.id, where, units or self.units),
            "Unable to command absolute move")

    def slew(self, rate, units=None):
        raise_status(
            mcSlewUnits(self.id, rate, units or self.units),
            "Unable to slew motor")

    def stop(self):
        raise_status(
            mcStop(self.id),
            "Unable to stop motor")

    def on(self, event):
        return Event(self, event)

    def load_firmware(self, filename):
        cdef String buf = bufferFromString(filename)
        status = mcFirmwareLoad(self.id, &buf);

        if status != 0:
            raise RuntimeError(status)

    def load_microcode(self, filename):
        cdef String buf = bufferFromString(filename)
        status = mcMicrocodeLoad(self.id, &buf);

        if status != 0:
            raise RuntimeError(status)

    def search(cls, driver):
        cdef String dri = bufferFromString(driver)
        cdef String results
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
        MDRIVE_IO_INVERT,
        MDRIVE_IO_DRIVE,

        MDRIVE_ENCODER,
        MDRIVE_VARIABLE,
        MDRIVE_EXECUTE

cdef class MdriveMotor(Motor):

    property address:
        def __get__(self):
            cdef String buf
            cdef int status
            status = mcQueryString(self.id, MDRIVE_ADDRESS, &buf)
            if status != 0:
                raise RuntimeError(status)
            return buf.buffer.decode('latin-1')

        def __set__(self, address):
            cdef String buf = bufferFromString(address[0])
            cdef int status
            status = mcPokeString(self.id, MDRIVE_ADDRESS, &buf)

            if status != 0:
                raise RuntimeError(status)

    property baudrate:
        def __get__(self):
            cdef int val, status
            status = mcQueryInteger(self.id, MDRIVE_BAUDRATE, &val)

            if status != 0:
                raise RuntimeError(status)
            else:
                return val

        def __set__(self, rate):
            cdef int status
            status = mcPokeInteger(self.id, MDRIVE_BAUDRATE, rate)

            if status != 0:
                raise RuntimeError(status)

    property serial:
       def __get__(self):
            cdef String buf
            raise_status(mcQueryString(self.id, MDRIVE_SERIAL, &buf),
                "Unable to fetch serial number")
            return buf.buffer.decode('latin-1')

    property part:
        def __get__(self):
            cdef String buf
            raise_status(mcQueryString(self.id, MDRIVE_PART, &buf),
                "Unable to fetch part number")
            return buf.buffer.decode('latin-1')

    property firmware:
        def __get__(self):
            cdef String buf
            raise_status(mcQueryString(self.id, MDRIVE_FIRMWARE, &buf),
                "Unable to fetch firmware version")
            return buf.buffer.decode('latin-1')

    property encoder:
        def __get__(self):
            cdef int val
            raise_status(
                mcQueryInteger(self.id, MDRIVE_ENCODER, &val),
                "Unable to fetch device encoder settings")
            return not not val

        def __set__(self, val):
            raise_status(
                mcPokeInteger(self.id, MDRIVE_ENCODER, val),
                "Unable to set device encoder setting")

    def name_set(self, address, serial_number):
        cdef String addr = bufferFromString(address[0])
        cdef String sn = bufferFromString(serial_number)
        cdef int status
        print("Calling mcPokeStringItem")
        status = mcPokeStringItem(self.id, MDRIVE_NAME, &addr, &sn)
        raise_status(status, "Unable to name motor")

    def search(cls):
        return Motor.search('mdrive')
    search = classmethod(search)

    def call(self, label):
        cdef String _label = bufferFromString(label)
        mcPokeString(self.id, MDRIVE_EXECUTE, &_label)

    def read(self, variable):
        cdef int value
        cdef String var = bufferFromString(variable)
        mcQueryIntegerItem(self.id, MDRIVE_VARIABLE, &value, &var)
        return value

    def write(self, variable, value):
        cdef String var = bufferFromString(variable)
        mcPokeIntegerItem(self.id, MDRIVE_VARIABLE, value, &var)

from libc.stdio cimport fflush, stdout
cdef void pyEventCallback(event_info_t * info) with gil:
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
        self.reset()

    def __dealloc__(self):
        mcUnsubscribe(self.motor.id, self.id)

    def reset(self):
        self.isset = False
        self.data = None
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
        mcEventWait(self.motor.id, self.event)
