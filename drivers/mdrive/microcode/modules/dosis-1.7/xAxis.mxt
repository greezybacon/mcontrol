'[VARIABLES]
VA Q1 = 0
VA PD = 0

'[SETTINGS]
EE 1
NE 1 
ES 3
BD 11
A 8192
D 16384
DB 50
VM 4096
VI 400
RC 100
HC 50
SF 600
S1 1,0,0
S2 16,0,0
S3 16,0,0
'[PROGRAMS]
PG 100
LB AA 
	PR "\$Revision: 1.22 $" 
	E

'Zero to home routine
LB M1
	ST 0
	ER 0
	HM 3
	H
	P 0
	BR G9, ST<>0
	ST 0
	ER 0
	BR G8

' Programmed Move
LB G1
' R1 holds attempt count
	R1 0
	PD 0
LB G2
' if attempts greater than three exit failure
	BR G9, R1>3
	ST 0
	ER 0
' Move to Q1 variable value
	MA Q1
' Wait till stall or complete
	H
' check dead band
	PD = Q1 - P
        CL ZC, PD < 0
        BR G8, PD <=DB

' Increment attempt count
	IC R1
	H 100
' if stalled try again
	BR G2, ST<>0
' else exit success
	BR G8
	E

' This program shows mo
LB G3
	R1 0
LB G4
	IC R1
	PR "Position ", P, " Stall ", ST
	ST 0
	ER 0
	MA Q1
	H
	BR G4, ST<>0
	DC R1
	PR "Total Stalls ", R1
	BR G8
	
' Exit Success
LB G8
	PR "1"
	E

' Exit Failure
LB G9
	PR "0"
	E

'ABS Function 
LB ZC
 PD = PD * -1
 RT
E

PG

' Dosis software 1.7 expects EM=1 and CK=0
CK=0
EM=1

S
'[END]
