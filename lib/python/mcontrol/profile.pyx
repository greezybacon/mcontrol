cdef extern from "motor.h":
    ctypedef struct measurement:
        unsigned long long value
        int units
    ctypedef union raw_measure:
        measurement measure
        unsigned long long raw
    ctypedef struct Profile:
        raw_measure accel
        raw_measure decel
        raw_measure vmax
        raw_measure vstart
        raw_measure deadband
        unsigned char current_run
        unsigned char current_hold
        raw_measure slip_max

cimport mcontrol.constants as k

cdef extern from "lib/client.h" nogil:
    int mcProfileGet(int motor, Profile * profile)
    int mcProfileGetAccel(int motor, Profile * profile,
        int units, int * value)
    int mcProfileGetDecel(int motor, Profile * profile,
        int units, int * value)
    int mcProfileGetMaxV(int motor, Profile * profile,
        int units, int * value)
    int mcProfileGetInitialV(int motor, Profile * profile,
        int units, int * value)
    int mcProfileGetMaxSlip(int motor, Profile * profile,
        int units, int * value)
    int mcProfileGetDeadband(int motor, Profile * profile,
        int units, int * value)
    int mcProfileSet(int motor, Profile * profile)

cdef class MotorProfile(object):

    cdef Profile _profile
    cdef Motor motor

    def __init__(self, motor):
        self.motor = motor

    def reset(self):
        cdef int status = mcProfileSet(self.motor.id, &self._profile)
        raise_status(status, "Unable to get motor profile")

    def commit(self):
        cdef int status = mcProfileSet(self.motor.id, &self._profile)
        raise_status(status, "Unable to set motor profile")

    def _get_value_units(self, value):
        units = None
        if type(value) is tuple:
            if len(value) != 2:
                raise ValueError("Two item tuple required: "
                    "(val, units)")
            value, units = value

            if type(units) is not int:
                if units not in all_units:
                    raise ValueError("Unsupported units")
                units = all_units[units]
        return value, units

    property accel:
        def __get__(self):
            cdef int value
            raise_status(
                mcProfileGetAccel(self.motor.id, &self._profile,
                    self.motor.units, &value),
                "Unable to retrieve profile acceleration")
            return value

        def __set__(self, value):
            value, units = self._get_value_units(value)
            self._profile.accel.measure.value = value
            self._profile.accel.measure.units = units or self.motor.units

    property decel:
        def __get__(self):
            cdef int value
            raise_status(
                mcProfileGetDecel(self.motor.id, &self._profile,
                    self.motor.units, &value),
                "Unable to retrieve profile deceleration")
            return value

        def __set__(self, value):
            value, units = self._get_value_units(value)
            self._profile.decel.measure.value = value
            self._profile.decel.measure.units = units or self.motor.units

    property vmax:
        def __get__(self):
            cdef int value
            raise_status(
                mcProfileGetMaxV(self.motor.id, &self._profile,
                    self.motor.units, &value),
                "Unable to retrieve profile max velocity")
            return value

        def __set__(self, value):
            value, units = self._get_value_units(value)
            self._profile.vmax.measure.value = value
            self._profile.vmax.measure.units = units or self.motor.units

    property vstart:
        def __get__(self):
            cdef int value
            raise_status(
                mcProfileGetInitialV(self.motor.id, &self._profile,
                    self.motor.units, &value),
                "Unable to retrieve profile max velocity")
            return value

        def __set__(self, value):
            value, units = self._get_value_units(value)
            self._profile.vstart.measure.value = value
            self._profile.vstart.measure.units = units or self.motor.units

    property slip:
        def __get__(self):
            cdef int value
            raise_status(
                mcProfileGetMaxSlip(self.motor.id, &self._profile,
                    self.motor.units, &value),
                "Unable to retrieve profile max velocity")
            return value

        def __set__(self, value):
            value, units = self._get_value_units(value)
            self._profile.slip_max.measure.value = value
            self._profile.slip_max.measure.units = units or self.motor.units

    property hold_current:
        def __get__(self):
            return self._profile.current_hold

        def __set__(self, value):
            self._profile.current_hold = value

    property run_current:
        def __get__(self):
            return self._profile.current_run

        def __set__(self, value):
            self._profile.current_run = value
