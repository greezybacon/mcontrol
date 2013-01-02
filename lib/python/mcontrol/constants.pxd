cdef extern from "motor.h":
    ctypedef enum motor_query:
        MCPOSITION
        MCMOVING
        MCSTALLED

        MCPROFILE
        MCACCEL
        MCDECEL
        MCVMAX
        MCVINITIAL
        MCDEADBAND
        MCRUNCURRENT
        MCHOLDCURRENT
        MCSLIPMAX

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

