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
SF=100
DE=1
JE=0
TE=0
HC=25
RC=75
HT=50
MT=20
A=100000
D=100000
VI=100
VM=4096

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

VA Q1
VA J=0          'Start  Jitter
                '0 = false, this does not stop Jitter
                '1 = true, Start Jittering for JC Cycles or New I
                '2 = in jitter loops
                '999 = close loop
VA I=0          'Index to position during jitter loop
                ' Like R in normal loops.
VA BJ = 0       'Blister Jitter Steps
                '0 = do nothing
                '1 = going to I index position
                '2 = jitter (+)
                '3 = jitter (-)
                '4 = Jitter cycles finished goto JP
                '999 = closing down


VA X=204	'Rotor Lock Check Value
VA R=0		'Requested Position
VA T=0		'Target Position
VA PD=0		'Position to Target/Request Delta
VA PZ=0		'Last starting position

VA RE=0		'Rotor Lock Flag (Did a Rotor Lock occur)
		'	0 = ok 		FALSE
		'	1 = locked	TRUE
VA ET=0		'Error Tracking
		'	Did we have an erro the last time through the loop
		'	0 = No 		FALSE
		'	1 = Yes		TRUE

VA JS=50	'Jitter Drop / Step Value
VA JP=0		'Jitter Position
VA JT=0		'Jittter test position
VA JW=2		'Jitter Pause (to close bulk door)
VA JC=4         'jitter cycles
VA JM=1900	'Jitter Max Limit
VA JN=-1900	'Jitter Min Limit

'Move Profiles (HOMING)
VA A0=100000   		'Acceleration
VA B0=100000        	'Deceleration
VA V0=4096           	'V Max
VA W0=100            	'V Initial
VA F0=100           	'Following Error (SF)
VA X0=100            	'Rotor Stall
VA Z0=5           	'Dead Band
VA J0=75             	'Run Current
VA K0=50		'Hold Current

'Move Profiles (MOTION 1) (normal)
VA A1=150000        	'Acceleration
VA B1=100000        	'Deceleration
VA V1=15000          	'V Max
VA W1=100            	'V Initial
VA F1=512            	'Following Error (SF)
VA X1=20           	'Rotor Stall
VA Z1=5            	'Dead Band
VA J1=75        	'Run Currente
VA K1=5			'Hold Current

'Move Profiles  (MOTION 2) (jitter)
VA A2=230000    	'Acceleration
VA B2=2457600         	'Deceleration
VA V2=30000          	'V Max
VA W2=100            	'V Initial
VA F2=512           	'Following Error (SF)
VA X2=20           	'Rotor Stall
VA Z2=5           	'Dead Band
VA J2=75            	'Run Current
VA K2=5 		'Hold Current



PG 100
LB AA 
	PR "\$Revision: 1.38 $" 
	E
LB M1
        CL P0
	R1 0
	R2 0
	R3 0
	ST 0
	ER 0
	SL -2048
	H
        ST 0
        ER 0
' Find home for first side
	HM 3
	H
	BR G9, ST<>0
' Note using R1 as hold variable
	R1 P
	MR 1024
	H
        ST 0
        ER 0
' Find home from other side
	HM 1
	H
' Calculate middle
	R2 P-R1/2
	R3 R1 + R2
' Move to middle
	MA R3
	H
' Zero
	P 0
        ER 0
        ST 0
' Verify we see the home sensor
	BR G8, I1<>0
	BR G9, I1<>1
	E

' Programmed Move
LB G1
' R1 holds attempt count
	R1 0
        CL P1
LB G2
	ST 0
	ER 0
' if attempts greater than three exit failure
	BR G9, R1>9
' Move to Q1 variable value
	MA Q1
' Wait till stall or complete
	H
' Increment attempt count
	IC R1
' if stalled try again
	BR G2, ST<>0
' else exit success
	BR G8
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


'=========================================================================
'CALL JB        Jitter  BLISTER
'=========================================================================
LB JB
    OE EA
    BJ=0
    J = 0
    R1 = 0
    R = P
    I = P
    T = P
    JP = P
    R1 = 0
LB JL
    BR RI, I<>JP
    BR RO, J = 0
    BR RI, R1 > JC
    BR RJ, J = 1
    BR RP, BJ = 3
    BR RN, BJ = 2
    BR JL, BJ <> 999
    SL 0
    CL P1
E

'=========================================================================
'SUB RI         Index
'=========================================================================
LB RI
    J = 0
    BJ = 1
    R1 = 0
    JP = I
    R = I
    T = P
    CL P1
    CL ME
    PD= I - P
    CL AB,PD<0
    BR RI, PD > DB
    BJ = 0
    BR JL
E

'=========================================================================
'SUB RO         J=0 check
'=========================================================================
LB RO
    PD= I - P
    CL AB,PD<0
    BR RI, PD > DB
    H 1
    BR JL
E


'=========================================================================
'SUB RJ         Start Jitter
'=========================================================================
LB RJ
    CL P2
    R1 = 0
    J = 2       'reset wait for next jitter start
    BR RP
E

'=========================================================================
'SUB RP        Move Positive
'=========================================================================
LB RP
    BJ = 2  'Moving positive
    R = JP + JS
    T = P
    CL NM, R > JM
    CL ME

    BR JL
E
'=========================================================================
'SUB NM       Set Max
'=========================================================================
LB NM
    R = JM
    RT
E

'=========================================================================
'SUB RN         Move Negative
'=========================================================================
LB RN
    BJ = 3  'Moving Negative
    R = JP - JS
    T = P
    CL NN, R < JN
    CL ME
    IC R1
    BR JL
E
'=========================================================================
'SUB NN      Set Min
'=========================================================================
LB NN
    R = JN
    RT
E


'=========================================================================
'SUB ME		Motion Execute (complete /Rotor Lock)
'=========================================================================
LB ME
 ET=0
 CL M9,R<>T 	'Need to apply motion
 BR ME,ET=1	'Had an error try to get moving again
 PD=P-T		'Det difference between current position and targe
 CL AB,PD<0	'insure positive
 BR MJ, RE=1
 BR ME,PD>DB
 H 1
LB MJ
 RT
E
'=========================================================================
'SUB M9 	Issue MA (ABS R)
'=========================================================================
LB M9
 T=R
 RE=0
 ST=0
 ET=0
 PZ=P
 MA T
 RT
E


'=========================================================================
'SUB AB		ABS on PD Make Delta Positive
'=========================================================================
LB AB
 PD=PD*-1
 RT
E


'*************************************************************************
' ERROR SUBS
'*************************************************************************
'=========================================================================
'SUB EA       ERROR ON Event
'=========================================================================
LB EA
 CL E0,ER=86
 'add other codes here
 ER=0
 ST=0
 RT
E
'=========================================================================
'SUB E0       ERROR - 86
'=========================================================================
LB E0
 ET=1		'Error Track
 T=R+1			'Allows restart (MA & SL)
 PD=P-PZ		'Det difference between current position and
			'last stopped position
 CL AB,PD<0		'insure positive
 CL EB,PD<=X		'Rotor Lock
 PZ=P			'Reset for next stop
 RT
E
'=========================================================================
'SUB EB		Error Rotor Lock
'=========================================================================
LB EB
 R=0		'Set R and T to P prevent
 T=0		'Restating in MA or SL
 RE=1		'Mark Rotor Lock
 ET=0		'Clear Error Track as RE will govern
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
'=========================================================================
'CALL P2 	Set Motion Profile 2 (jitter)
'=========================================================================
LB P2
 A = A2
 D = B2
 VM = V2
 VI = W2
 SF = F2
 X = X2
 DB = Z2
 RC = J2
 HC = K2
 RT
E

PG

' Dosis software 1.7 expects EM=1 and CK=0
CK=0
EM=1

S
