
'Setup
CE=2
DG=1
ES=3
LK=0
QD=0

'Motion
EE=1
MS=256
NE=0
PM=0
SM=0
DB=10
SF=1200
DE=1
JE=0
TE=0
HC=25
RC=100
HT=50
MT=20
A=20480
D=40000
VI=200
VM=15360

'IO
OL 0
OT 0
D1=1
D2=1
D3=1
D4=1
S1=1,0,0
S2=0,0,0
S3=0,0,0
S4=17,0,0

'Move Profiles (HOMING)
VA A0=20480   		'Acceleration
VA B0=100000        	'Deceleration
VA V0=4096           	'V Max
VA W0=200            	'V Initial
VA F0=1200           	'Following Error (SF)
VA Z0=5           	'Dead Band
VA J0=100             	'Run Current
VA K0=100		'Hold Current

'Move Profiles (MOTION 1) (normal)
VA A1=10240        	'Acceleration
VA B1=40000        	'Deceleration
VA V1=15360          	'V Max
VA W1=200            	'V Initial
VA F1=1200            	'Following Error (SF)
VA Z1=5            	'Dead Band
VA J1=100        	'Run Currente
VA K1=50		'Hold Current

'Move Profiles (MOTION 2) (slow -- short distances)
VA A2=10240        	'Acceleration
VA B2=10240        	'Deceleration
VA V2=15360          	'V Max
VA W2=200            	'V Initial
VA F2=1200            	'Following Error (SF)
VA Z2=5            	'Dead Band
VA J2=100        	'Run Currente
VA K2=50		'Hold Current

'Move Profiles (MOTION 3) (fast -- long, downward distances)
VA A3=158141        	'Acceleration (0.3 g)
VA B3=105428        	'Deceleration (0.2 g)
VA V3=27307          	'V Max (1000rpm)
VA W3=200            	'V Initial
VA F3=1200            	'Following Error (SF)
VA Z3=5            	'Dead Band
VA J3=50        	'Run Currente
VA K3=50		'Hold Current

VA Q1
VA PD = 0
VA PZ = 0
VA PT = 256
VA SP = 200





PG 100


LB AA
	PR "\$Revision: 1.27 $"
	E

' Include the 'TB' routine to trust the brake and release the coils (DE=0)
#include "../brake.mxt"


LB M1
  CL P0
  ' Clear previous trip on time (8), if any
  TE = TE & !8
  DE = 1
  ' if we are not in home do routine
  BR M2, I1<>1
  ' if we are in home more off
  MR -2048
  H
LB M2
	ST 0
	ER 0
	HM 3
	H
	BR G8, ST<>1 	
	BR G9

''
' SR
' Select Profile
'
' Selects and loads a profile appropriate for the move scheduled in Q1.
' These profiles will be used
'
'  Distance              | Selected profile
' -----------------------+----------------------
'  |dP| < 6144 (4.5")    | P2 (slow)
'  dP > 6144             | P3 (fast-downward)
'  dP < -6144            | P1 (normal, upward)
'
LB SR
  ' Calculate the change in position. Positive is downward.
  R4 = Q1 - P

  CL P3, R4 > 6144
  CL P1, R4 < -6144

  ' Compute the absolute value of the distance, too
  BR S0, R4 > 0
  ' if (R4 <= 0) {
    R4 = R4 * -1
  ' }
  LB S0

  CL P2, R4 <= 6144

RT

' Programmed Move
LB G1
' R1 holds attempt count
  ' Clear previous trip on time (8), if any
  TE = TE & !8
  DE = 1

  ' Use profile 1, 2, or 3 based on distance
  CL SR
 
	R1 0
	PD = 0
	PZ = P
	BR G2
	E
LB G7
	HC 100
	H SP
LB G2
	ST 0
	ER 0
' if attempts greater than three exit failure
	BR G9, R1>3
' Move to Q1 variable value
	MA Q1
' Wait till stall or complete
	H
' check dead band
	PD = Q1 - P
        CL ZC, PD < 0
        BR G8, PD <=DB
' check to see if travled enough
	PD = P -PZ
	CL ZC, PD < 0
	CL ZD, PD > PT
	PZ = P 
' Increment attempt count
	IC R1
' if stalled try again
	BR G7, ST<>0
' else exit success
	BR G8
	E

' Move Data
LB G3
	ST 0
	ER 0
	PR "	Start Position- ", P
	MA Q1
LB G4
	H 1
	R2=C1/25
	R3=R2-C2
	PR "    Position - ", P, "	Error - ",R3
	BR G4, MV<>0
	PR "	End Position - ", P
	RT
	E

' Exit Success
LB G8
  CL P1
  PR "1"
  TT = 40000, TB
  TE = TE | 8
E

' Exit Failure
LB G9
  CL P1
  PR "0"
  TT = 40000, TB
  TE = TE | 8
E

'ABS Function 
LB ZC
 PD = PD * -1
 RT
E
'Reset R1
LB ZD
 R1 = 0
 RT
E



'*************************************************************************
' Motion Profiles
'*************************************************************************
'=========================================================================
'CALL P0 	Set Motion Profile 0 (HOMING)
'=========================================================================
LB P0
  A = A0
  D = B0
  VM = V0
  VI = W0
  SF = F0
  DB = Z0
  RC = J0
  HC = K0
RT
'=========================================================================
'CALL P1 	Set Motion Profile 1(running)
'=========================================================================
LB P1
  A = A1
  D = B1
  VM = V1
  VI = W1
  SF = F1
  DB = Z1
  RC = J1
  HC = K1
RT

'=========================================================================
'CALL P2 	Set Motion Profile 2(short haul)
'=========================================================================
LB P2
  A = A2
  D = B2
  VM = V2
  VI = W2
  SF = F2
  DB = Z2
  RC = J2
  HC = K2
RT

'=========================================================================
'CALL P3 	Set Motion Profile 3(long downward haul)
'=========================================================================
LB P3
  A = A3
  D = B3
  VM = V3
  VI = W3
  SF = F3
  DB = Z3
  RC = J3
  HC = K3
RT


PG

' Dosis software 1.7 expects EM=1 and CK=0
CK=0
EM=1

S
