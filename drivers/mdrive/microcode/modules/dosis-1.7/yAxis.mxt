

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
HC=5
RC=100
HT=0
MT=0
A=10240
D=10240
VI=200
VM=30720

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
S4=0,0,0

'Move Profiles (HOMING)
VA A0=4096  		'Acceleration
VA B0=80000       	'Deceleration
VA V0=3072          	'V Max
VA W0=200            	'V Initial
VA F0=1200           	'Following Error (SF)
VA X0=50            	'Rotor Stall
VA Z0=5           	'Dead Band
VA J0=100             	'Run Current
VA K0=5 		'Hold Current

'Move Profiles (MOTION 1) (normal)
VA A1=10240        	'Acceleration
VA B1=10240        	'Deceleration
VA V1=30720          	'V Max
VA W1=200            	'V Initial
VA F1=1200            	'Following Error (SF)
VA X1=50           	'Rotor Stall
VA Z1=5            	'Dead Band
VA J1=100        	'Run Currente
VA K1=5 		'Hold Current


VA Q1
VA PD = 0
VA PZ = 0
VA PT = 256
VA SP = 200





PG 100


LB AA
	PR "\$Revision: 1.26 $"
	E



LB M1
  CL P0
  ' if we are not in home do routine
  BR M2, I1<>1
  ' if we are in home more off
  MR 2048
  H
LB M2
	ST 0
	ER 0
	HM 1
	H
	BR G8, ST<>1 	
	BR G9


' Programmed Move
LB G1
' R1 holds attempt count
	HC 100
	R1 0
	PD = 0
	PZ = P
	BR G2
	E
LB G7
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
	PR "    Position - ", P "	Error - ",R3
	BR G4, MV<>0
	PR "	End Position - ", P
	RT
	E

' Exit Success
LB G8
  CL P1
  PR "1"
E

' Exit Failure
LB G9
  CL P1
  PR "0"
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
 X = X0
 DB = Z0
 RC = J0
 HC = K0
 RT
E
'=========================================================================
'CALL P1 	Set Motion Profile 1(running)
'=========================================================================
LB P1
 A = A1
 D = B1
 VM = V1
 VI = W1
 SF = F1
 X = X1
 DB = Z1
 RC = J1
 HC = K1
 RT
E

PG

' Dosis software 1.7 expects EM=1 and CK=0
CK=0
EM=1

S
