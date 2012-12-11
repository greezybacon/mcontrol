cdef extern from "motor.h":
    ctypedef enum motor_query:
        MCPOSITION
        MCMOVING

    ctypedef enum motion_increment:
        MILLI_INCH
        INCH
        MILLI_METER
        METER
        MILLI_DEGREE
        MILLI_ROTATION
        MILLI_RADIAN

        MILLI_G

        MICRO_REVS

