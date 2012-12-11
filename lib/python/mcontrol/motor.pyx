# cython: profile=True

cimport mcontrol.constants as k

all_units = {
    k.MILLI_INCH:   "mil",
    k.INCH:         "inch",
    k.MILLI_METER:  "mm",
    k.METER:        "m",
    k.MILLI_G:      "milli-g",
    k.MILLI_DEGREE: "milli-deg",
    k.MILLI_ROTATION: "milli-rev"
}

class NoDaemonException(Exception): pass
class CommFailure(Exception): pass

cdef class Motor:

    cdef int id
    cdef int _units
    cdef long long _scale

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

    property moving:
        def __get__(self):
            cdef int status, moving
            status = mcQueryInteger(self.id, k.MCMOVING, &moving)

            if status != 0:
                raise RuntimeError(status)

            return not not moving

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

cdef class MdriveMotor(Motor):

    property address:
        def __get__(self):
            cdef String buf
            cdef int status
            status = mcQueryString(self.id, 10007, &buf)
            if status != 0:
                raise RuntimeError(status)
            return buf.buffer.decode('latin-1')

        def __set__(self, address):
            cdef String buf = bufferFromString(address[0])
            cdef int status
            status = mcPokeString(self.id, 10007, &buf)

            if status != 0:
                raise RuntimeError(status)

    property baudrate:
        def __get__(self):
            cdef int val, status
            status = mcQueryInteger(self.id, 10004, &val)

            if status != 0:
                raise RuntimeError(status)
            else:
                return val

        def __set__(self, rate):
            cdef int status
            status = mcPokeInteger(self.id, 10004, rate)

            if status != 0:
                raise RuntimeError(status)

    def name_set(self, address, serial_number):
        cdef String addr = bufferFromString(address[0])
        cdef String sn = bufferFromString(serial_number)
        cdef int status
        print("Calling mcPokeStringItem")
        status = mcPokeStringItem(self.id, 10008, &addr, &sn);
        raise_status(status, "Unable to name motor")

    def search(cls):
        return Motor.search('mdrive')
    search = classmethod(search)
