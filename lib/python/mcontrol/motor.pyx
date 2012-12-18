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

    property profile:
        def __get__(self):
            if not self._has_profile:
                self._profile = MotorProfile(self)
                self._has_profile = True
            return self._profile

    def move(self, how_much, units=None):
        if units is None:
            units = self.units

        cdef int status
        status = mcMoveRelativeUnits(self.id, how_much, units)

        if status != 0:
            raise RuntimeError(status)

    def moveTo(self, where, units=None):
        if units is None:
            units = self.units

        cdef int status
        status = mcMoveAbsoluteUnits(self.id, where, units)

        if status != 0:
            raise RuntimeError(status)

    def slew(self, rate, units=None):
        if units is None:
            units = self.units

        raise_status(
            mcSlewUnits(self.id, rate, units),
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

        MDRIVE_ENCODER

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
        status = mcPokeStringItem(self.id, 10008, &addr, &sn)
        raise_status(status, "Unable to name motor")

    def search(cls):
        return Motor.search('mdrive')
    search = classmethod(search)

cdef void pyEventCallback(event_info_t * info) with gil:
    assert info.user != NULL
    assert type(<object>info.user) is Event
    cdef object data = None
    if info.event == EV_MOTION and info.data:
        data = mdFromEventData(info.data)
    (<object>info.user).callback(data)

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
    self.completed = data.motion.completed
    self.stalled = data.motion.stalled
    self.cancelled = data.motion.cancelled
    self.stopped = data.motion.stopped
    self.in_progress = data.motion.in_progress
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
