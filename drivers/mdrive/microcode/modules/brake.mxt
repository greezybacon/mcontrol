#define BRAKE

''
' TB
' Test for brake failure
'
' This is implemented in two phases. First, if brake failure has not been
' detected, then the motor can be safely disabled after a move operation,
' and a relative time trip is installed to catch the motor if it falls
' because of a brake failure.
'
' This routine should not be called directly. Instead, schedule it to be
' called after some reasonable amount of time with the TT instruction.
' Anything over 50ms should be sufficient for the brake to engage.
'
' Context:
' FB = Status of brake -- 0 is ok, >0 is failed
' R4 = Initial status of move completion -- 1 for success, 0 for failure
'
VA FB = 0     ' Failed Brake
LB TB
  ' Cache value of Q1 for sensing a race in the WB routine. Q1 should only
  ' change when this routine is called
  R3 = Q1

  ' If brake is already determined to have failed, skip ahead without
  ' scheduling the trip callback or disabling the motor coils
  BR T1, FB > 0
  ' if (FB == 0) {

    ' Disable motor coils after brake has time to engage
    DE = 0

    ' Trip after 20ms and check if the brake failed
    TT = 20, WB
    ' Enable trip-on-time (8)
    TE = TE | 8

  ' }
  LB T1
' RT is required for trip routines
RT            ' End of TB

''
' WB
' Watch for failed brake
'
' Used as a trip target for relative move. If the motor is moving and trips
' the relative position and the coils are not energized, then the motor
' brake has failed. If the brake has failed, the coils and hold current
' should be restored to the device to prevent the EOAT from immenant peril
'
' Context:
' G1/Q1 = G1 is called again to return to previous destination if brake
'   failure is set
' FB = Failed brake, set to 2 if brake is deemed as failed
'
LB WB
  ' If the motor coils are enabled, there's no need to check for brake
  ' failure.
  BR W8, DE = 1

  ' Handle possible race where Q1 has been changed for a move, but this
  ' time trip routine was invoked before the G1 (Move) routine.
  ' HINT: The Q1 value is saved into R3 at the end of a move in TB.
  '
  ' Should Q1 change, abort the brake watching and assume that another move
  ' is about to begin, which will retrigger the brake watch loop
  BR W4, R3 <> Q1

  ' Calculate the distance fallen (if any)
  R4 = P - Q1

  ' See if we fell over DB
  BR W8, R4 <= 50

  ' if (DE == 0 && FELL > DB) {

    ' Brake has failed!
    FB = 1

    ' Enable the coils to halt the motor
    HC = 100
    RC = 100
    DE = 1

    ' Ask the device to slew at the approximate rate of fall
    ' v = g * t, t is assumed here at 20ms, g = 386 in/s2,
    ' or 386 / 1.5 * 2048 ticks / s2 or 527138 ticks/s2. For 20ms,
    ' velocity should be 10542. However, since the unit is already moving
    ' quickly, use acceleration close to 1g and VI should be assumed at the
    ' current rate of fall (minus a little to account for timing error, and
    ' mechanical drag)
    VI = 8000
    ' Keep up with gravity, and try not to fall too far
    A = 368996      ' 0.7g
    D = 105428      ' 0.2g
    ST = 0
    SL 10542

    ' Allow the motor to catch the EOAT, stop, and move back to where the
    ' last move was requested
    H 20
    SL 0
    H

    ' Wait for move to finish and reset the profile to defaults
    MA Q1
    H

    ' Quit watching if the brake failed
    BR W4
  ' }
  LB W8
  ' else {
    ' Brake is holding, check again in 25ms (40Hz)
    TT = 25, WB
    TE = TE | 8

  ' }
  LB W4

' RT is required for trip routines
RT            ' End of WB

' vim: se ft=vb:
