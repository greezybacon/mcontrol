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

    def accel_get(self, units=k.MICRO_REVS):
        cdef int value
        raise_status(
            mcProfileGetAccel(self.motor.id, &self._profile,
                units, &value),
            "Unable to retrieve profile acceleration")
        return value

    def accel_set(self, value, units=k.MICRO_REVS):
        self._profile.accel.measure.value = value
        self._profile.accel.measure.units = units

    def decel_set(self, value, units=k.MICRO_REVS):
        self._profile.decel.measure.value = value
        self._profile.decel.measure.units = units

    def vmax_set(self, value, units=k.MICRO_REVS):
        self._profile.vmax.measure.value = value
        self._profile.vmax.measure.units = units

    def vstart_set(self, value, units=k.MICRO_REVS):
        self._profile.vstart.measure.value = value
        self._profile.vstart.measure.units = units

    def slip_set(self, value, units=k.MICRO_REVS):
        self._profile.slip_max.measure.value = value
        self._profile.slip_max.measure.units = units

    def hold_current_set(self, value):
        self._profile.current_hold = value

    def run_current_set(self, value):
        self._profile.current_run = value
