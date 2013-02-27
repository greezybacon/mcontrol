VA T=0      ' Target Position
VA OF=0     ' Original stall-factor (at beginning of motion loop)
VA FE=0     ' Following error (current)
VA TS=0     ' Tries recorded for a move request

''
' Function: MI
'
' Motion init. Preps the unit for a move. Ensures that all the internal
' settings are ready for the move
'
LB MI
    DE = 1          ' Energize the coils
    SL 0            ' Stop moving
    OF = SF         ' Record current stall factor
    FE = 0          ' Reset following error
    TS = 0          ' Reset tries counter
    R4 = C1 / 25 - C2 ' Calculate initial following error
    H
    RT
E

''
' Function: MO
'
' Move to absolute or relative position. Set mode with R1, and amount with
' R2
'
' Parameters:
' R1 - Mode of operation (packed):
'       struct operation {
'           /* LSB */
'           unsigned    mode            :3;
'           unsigned    profile         :3;
'           unsigned    reset_position  :1;
'           unsigned    notify          :8;
'       };
'       1: Move absolute to given position
'       2: Move relative to given position
'       3: Slew at given rate
'       n << 4: (mask) Use profile n
'       64: (mask) Reset position before slew
'       n << 7: (mask) Notify at position (as 1/256 of total requested
'               distance traveled)
' R2 - Amount for the operation (rate for R1=3, steps otherwise)
'
#define move_label "MO"
#define following_error "FE"
LB MO
    CL MI           ' Prep for a new move

' Use given profile (if set and supported)
#if "profiles" in vars()
  R3 = R1 / 8 & 8
  #if profiles > 0
    CL P0, R3 = 1
  #endif
  #if profiles > 1
    CL P1, R3 = 2
  #endif
  #if profiles > 2
    CL P2, R3 = 3
  #endif
  #if profiles > 3
    CL P3, R3 = 4
  #endif
#endif

    ' Use slew routine if slew is requested
    R3 = R1 & 7
    BR M3, R3 = 3

    ' Convert relative move to absolute
    BR M1, R3 = 1

    ' Add current position to the requested relative amount
    R2 = R2 + P

  LB M1
    ' Move to requested position (absolute)
    T = R2

  LB M2
    ' (Continue) moving toward last-requested target
    IC TS

    ' Setup the trap for callback, if requested
#if "notify-hw-support" in vars()

    ' Isolate the .notify member of the R1 packed struct
    R3 = R1 / 128 & 255
    BR M6, R3 = 0

    R3 = T - P * R3 / 256 + T
    TP R3, OE

  LB M6
#endif

    MA T
    BR ML

  LB M3
    BR M4, R1 & 64 = 0
    P = 0           ' Reset position to keep from counter overflow

  LB M4
    SL R2           ' Slew at requested speed
    BR ML

''
' Function: ML
' Main motion loop. Collect information about the unit's travel and
' autocorrect stall factor, etc.
'
' Context:
' R4 - Initial following error (difference between C1 and C2)
' OF - Stall factor margin (added to the continuous following error to
'      calculate continuous stall factor
' T - Target position
'
LB ML

    ' Compute following error into R3
    R3 = C1 / 25 - C2 - R4

    ' Following error is always positive
    BR M5, R3 >= 0
    R3 = R3 * -1

  LB M5
    FE = R3

    ' Adjust the stall factor actively
    SF = OF + FE

    ' ------------- End of loop (ML) --------------
    H 10
    BR ML, MV & !ST

    ' Reset the stall factor to the original stall factor
    SF = OF
E
