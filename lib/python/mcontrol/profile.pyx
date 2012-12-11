cdef extern from "motor.h":
    ctypedef struct Profile:
        pass
    ctypedef struct measurement:
        pass

cimport mcontrol.constants as k

cdef extern from "lib/client.h" nogil:
    int mcProfileGet(int motor, Profile * profile)
    int mcProfileGetAccel(int motor, Profile * profile,
        int units, int * value)
    int mcProfileSet(int motor, Profile * profile)

cdef class MotorProfile:

    cdef Profile _profile

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
