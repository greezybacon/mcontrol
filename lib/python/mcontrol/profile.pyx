cdef extern from "motor.h":
    ctypedef struct measurement:
        unsigned long long value
        int units
    ctypedef struct profile_attrs:
        unsigned hardware
        unsigned number
        unsigned refresh
        unsigned loaded
    ctypedef struct Profile:
        measurement accel
        measurement decel
        measurement vmax
        measurement vstart
        measurement deadband
        unsigned char current_run
        unsigned char current_hold
        measurement slip_max
        profile_attrs attrs

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
    cdef Motor _motor

    def __init__(self, motor):
        self._motor = motor

    cdef void use_profile(MotorProfile self, Profile profile):
        self._profile = profile

    property motor:
        def __get__(self):
            return self._motor
        def __set__(self, motor):
            if not isinstance(motor, Motor):
                raise TypeError("Motor instance expected")
            self._motor = motor

    def reset(self):
        """
        Forces local values to reflect those currently set in the motor
        """
        cdef int status
        with nogil:
            status = mcProfileGet(self.motor.id, &self._profile)
        raise_status(status, "Unable to get motor profile")

    def commit(self):
        """
        Flush local profile changes to the motor
        """
        cdef int status
        with nogil:
            status = mcProfileSet(self.motor.id, &self._profile)
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
            return (self._profile.accel.value,
                    self._profile.accel.units)

        def __set__(self, value):
            value, units = self._get_value_units(value)
            self._profile.accel.value = value
            self._profile.accel.units = units or self.motor.units

    property decel:
        def __get__(self):
            return (self._profile.decel.value,
                    self._profile.decel.units)

        def __set__(self, value):
            value, units = self._get_value_units(value)
            self._profile.decel.value = value
            self._profile.decel.units = units or self.motor.units

    property vmax:
        def __get__(self):
            return (self._profile.vmax.value,
                    self._profile.vmax.units)

        def __set__(self, value):
            value, units = self._get_value_units(value)
            self._profile.vmax.value = value
            self._profile.vmax.units = units or self.motor.units

    property vstart:
        def __get__(self):
            return (self._profile.vstart.value,
                    self._profile.vstart.units)

        def __set__(self, value):
            value, units = self._get_value_units(value)
            self._profile.vstart.value = value
            self._profile.vstart.units = units or self.motor.units

    property slip:
        def __get__(self):
            return (self._profile.slip_max.value,
                    self._profile.slip_max.units)

        def __set__(self, value):
            value, units = self._get_value_units(value)
            self._profile.slip_max.value = value
            self._profile.slip_max.units = units or self.motor.units

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

    property hardware:
        """
        Gets or sets which profile slot this number is in the motor's
        hardware or microcode. A value of zero indicates that the profile is
        not reflected in hardware. Valid values are positive, 1-based
        numbers
        """
        def __get__(self):
            return self._profile.attrs.number
        def __set__(self, slot):
            if type(slot) is not int:
                raise TypeError("Integer slot number expected")
            elif slot < 1:
                raise ValueError("Positive, 1-based slot number expected")
            self._profile.attrs.hardware = True
            self._profile.attrs.number = slot

    def copy(self):
        copy = MotorProfile(self._motor)
        copy.use_profile(self._profile)
        return copy

    def __repr__(self):
        return self.__unicode__()
    def __unicode__(self):
        attrs = ('accel', 'decel', 'vstart', 'vmax', 'slip')
        values = []
        for a in attrs:
            v, u = getattr(self, a)
            if u != self.motor.units:
                values.append("{0}={1}{2}".format(a, v, all_units[u]))
            else:
                values.append("{0}={1}".format(a, v))
        return "Profile({0}, irun={1}, ihold={2})".format(
               ', '.join(values), self.run_current, self.hold_current)
