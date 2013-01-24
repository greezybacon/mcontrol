'Program Name:  	mdrive base program
'File name:		mdrive_base.mxt
'
'Description:  		base motor control 
'
'Revision:		"\$Revision: 1.18 $"

'
'
'=========================================================================
'Notes
'=========================================================================
'All LB-Labels MUST BE CAPS
'All VA-VARIABLES MUST BE CAPS
'Device Name DN must be small
'NOTE when sending commands down the wire only 
'       USE CAPS..... this will prevent party mode
'       mistakes if first char is lost 
'       i.e. sending "xPR MV" vs "xpr mv" if the p Motor
'	does not see the x it will set r to mv...
'	commands using all CAPS prevent this 
'	as p & P are different motor names 
'
'IO for P & O 
'IO 1 	0 - do nothing 
'       1 - triger
'IO 2	0 -No Jitter
'       1 - Jitter door 
'IO 3	0 - do not add
'	1 - add if last read was 0
'IO 4   0 - do not substract
'	1 - substrace if last read was 0
'
'I0 3 & 4 	1&1 reset Door Position

'OL 0 - 0000 reset (noJitter)
'OL 1 - 0001 Triger
'OL 2 - 0010 Jitter
'OL 5 - 0101 Just Increase
'OL 7 - 0111 Jitter with Increase
'OL 9 - 1001 Just descrease 
'OL 11- 1011 Jitter with Decrease
'OL 13- 1101 Reset 
'OL 15- 1111 Jitter with reset 


'R1
'R2	-count reg Jitter
'R3	-compute register
'R4

'=========================================================================
'Calls
'=========================================================================
'AA	Get version  
'AO	Acces Open to LO 
'AC	Acces Close to LC
'AJ	Acces Open to LO and start JL
'JL	Jitter Bulk Lift (PAxis)
'JD	Jitter Bulk Door (OAxis) 
'DA	Dosis Move ABS (MA R)
'DM	Dosis Move Relative (MA P+R)
'DS	Dosis Speed (SL R) 
'DZ	Dosis Loops Close 
'HH	Homing to Hard Stop
'P0	Set Motion Profile 0  Homing 
'P1 	Set Motion Profile 1  Motion 1
'P2 	Set Motion Profile 2  Motion 2
'P3 	Set Motion Profile 3  Motion 3
'=========================================================================
'SUBS
'=========================================================================
'GF		Find Jitter Point JP  
'GP		Jitter Lift Pause 
'GJ		Jitter Lift Jitter 
'G0		Jitter Lift Check the Need
'G1		Jitter Lift reset when see
'G2		Jitter Lift Cycle the Ramp
'G3		Jitter Lift Send Step the Door 
'G4		Jitter Lift Drop 
'G5		Jitter Lift Raise 
'G6		Jitter Lift Set Min 
'GZ		Jitter Lift Close (Not door Lift min pos)
'GY		Jitter Lift Closed Waiting 
'KV		Bulk Door Ensure at rest position Close
'KU		Bulk Door Do the Jitter 
'KW		Bulk Door Det direction Open
'KX		Bulk Door keep R below max
'KY		Bulk Door keep R above min 
'KT		Bulk Door Input Trip
'KC		Bulk Door Input Trip INCREASE
'KD		Bulk Door Input Trip DECREASE
'KE		Bulk Door Input Trip RESET
'MC		Motion CleanUP
'ML		Motion Loop  
'MI		Motion Decisions  
'MB		Motion Stop check Band    
'MX		Motion retry not at location 
'MZ		Motion Close   
'ME		Motion Execute (complete /Rotor Lock) 
'M1		Issue MA (ABS R)
'M2		Issue MA (with reletative R)
'M3		Issue SL 
'H0		Homing Hard Stop Loop 
'HY		Homing Complete Good 
'HX		Homing Complete Failed 
'HZ		Homing Finish (STOP EXIT)
'AB		ABS on PD Make Delta Positive 
'LS		LOOP SLEEP 
'ZZ		STOP PG
'EA       	ERROR ON Event 
'E0       	ERROR - 86
'EB		Error Rotor Lock
'=========================================================================
'Load Setup
'=========================================================================

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
DE=0
JE=0
TE=0
HC=5
RC=25
HT=500
MT=0
A=20480
D=20480
VI=100
VM=20480

'IO
OL 0
OT 0
D1=1
D2=1
D3=1
D4=1
S1=0,0,0
S2=0,0,0
S3=0,0,0
S4=0,0,0

'=========================================================================
'Variables
'=========================================================================
VA X=204	'Rotor Lock Check Value
VA R=0		'Requested Position
VA T=0		'Target Position
VA PD=0		'Position to Target/Request Delta
VA PZ=0		'Last starting position

VA RE=0		'Rotor Lock Flag (Did a Rotor Lock occur)
		'	0 = ok 		FALSE
		'	1 = locked	TRUE
 
VA HE=0		'Homing Error (Do you have an error)
		'	-1= started 	UNKNOWN
		'	0 = ok 		FALSE
		'	1 = Error 	TRUE
		'note if HE=-1 & BY=1 then Homing is running
		'if HE=-1 & BY=0 then Homing started and program exited
VA ET=0		'Error Tracking
		'	Did we have an erro the last time through the loop
		'	0 = No 		FALSE
		'	1 = Yes		TRUE
VA DD=0		'Dosis Motion Decisions Flag
		'	0 = unset
		'	1 = ABS Move
		'	    :R = ABS Position and is moved using MA
		'	2 = Relative Move
		'	    :R = is added to current then moves using MA
		'	3 = Slew speed
		'	    :R = Velocity
VA DR=-1	'Homing Direction
VA HL=30000	'Homing Limit for moving to Hard Stop
			'1.25 times of axis max for Canister as 
			'initial engagement is suspect. 
			'Always positive DR takes care of direction
			'Note Mid will move in both +/-.
VA JM=25500	'Jitter Max Limit
VA JN=12500	'Jitter Min Limit
VA JJ=1024	'Jitter Inital Drop Value
VA JS=512	'Jitter Drop Step Value
VA JP=0		'Jitter Position 
VA JT=0		'Jittter test position
VA JW=2		'Jitter Pause (to close bulk door)
VA LO=12200	'Access Door (LIFT) Open Position
VA LC=50	'Access Door (LIFT) Close Position
VA BZ=500	'Bulk Door Close
VA BM=750	'Bulk Door Lower Jitter Position 
VA KR=3		'Number of Door jitters to cycle before pause
VA KQ=90	'mSec to wait for bulk door pause in opening

VA BF=0		'Bulk Flag 
		'	0 = unset 
		'	1 = Pause (Have enough pills)
		'	2 = Need  (Need Pills)
		'	3 = See	  (See Pills falling)
		'	4 = Close (Have enough to finish)
		'	9 = Closed waiting next command
		'	999 = exit
VA LF=0		'Bulk Lift Flag
		'	0 = not set
		'	1 = Find Jitter Point JP
		'	2 = Hold
		'	3 = Jitter Need
		'	4 = Jitter See
		'	8 = closing 
		'	9 = close
VA LJ=0		'Lift Jitter Steps
		'	0 = Unknown 
		'	1 = need JP
		'	2 = Have JP
		'	3 = Lift Dropping
		'	4 = Lift Raising
		'	5 = Lift Cycling
VA DF=0		'Bulk Door Flag
		'	0 = not set
		'	999 - Exit
		'	
VA DJ=0		'Door Steps 
		'	0 = Unknown 
		'	1 = Try to Close
		'	2 = Try To Open
VA KF=0		'Bulk Door Flag for reading IO 
VA FF=0		'Current Frame Number
VA FS=0		'Frame Start current Bulk Door
VA FZ=0		'Frame Start current CYCLE
VA FA=15	'Frame Transition A (Door)
VA FB=90	'Frame Transition B (Ramp)
VA FP=0 	'Frame delta


'Move Profiles (HOMING)
VA A0=63000    		'Acceleration
VA B0=1200000        	'Deceleration
VA V0=8192           	'V Max
VA W0=100            	'V Initial
VA F0=24            	'Following Error (SF)
VA X0=24            	'Rotor Stall
VA Z0=5           	'Dead Band
VA J0=25             	'Run Current
VA K0=25		'Hold Current

'Move Profiles (MOTION 1) (open/close/drop)
VA A1=1200000        	'Acceleration
VA B1=1800000        	'Deceleration
VA V1=30720          	'V Max
VA W1=100            	'V Initial
VA F1=410            	'Following Error (SF)
VA X1=204            	'Rotor Stall
VA Z1=5            	'Dead Band
VA J1=100        	'Run Currente
VA K1=25		'Hold Current


'Move Profiles  (MOTION 2) (find JP)
VA A2=600000    	'Acceleration
VA B2=1200000         	'Deceleration
VA V2=8192          	'V Max
VA W2=100            	'V Initial
VA F2=24           	'Following Error (SF)
VA X2=24           	'Rotor Stall
VA Z2=5           	'Dead Band
VA J2=50            	'Run Current
VA K2=50		'Hold Current

'Move Profiles  (MOTION 3) (Lifting)
VA A3 = 600000    	'Acceleration
VA B3 = 4000000         'Deceleration
VA V3 = 32000          	'V Max
VA W3 = 100            	'V Initial
VA F3 = 100           	'Following Error (SF)
VA X3 = 48            	'Rotor Stall
VA Z3 = 5           	'Dead Band
VA J3 = 100            	'Run Current
VA K3 = 50		'Hold Current

'/////////////////////////////////////////////////////////////////////////////
'/////////////////////////////////////////////////////////////////////////////
'//////////////////////////////  PROGRAM  ////////////////////////////////////
'/////////////////////////////////////////////////////////////////////////////
'/////////////////////////////////////////////////////////////////////////////
PG 1000

'===================================================================================
'CALL AA  Get version  
'	Used by CVS to keep track of check in version
'	System will log this value on each motor ID event
'===================================================================================
LB AA
 PR "\$Revision: 1.18 $"
E
'=========================================================================
'CALL AO	Acces Open to LO 
'=========================================================================
LB AO
 OE EA
 OL 0
 CL MC		'Motion Cleanup 
 R=LO
 CL ME
 BR ZZ,RE=1     'Can not make it Stop
E
'=========================================================================
'CALL AC	Acces Close to LC 
'=========================================================================
LB AC
 OE EA
 OL 0
 BF=999
 CL MC		'Motion Cleanup 
 R=LC
 CL ME
 BR ZZ 
E
'=========================================================================
'CALL AJ	Acces Open to LO and start JL
'=========================================================================
LB AJ
 OE EA
 OL 0
 LJ=0
 BF=0
 CL MC		'Motion Cleanup 
 R=LO
 CL ME
 BR GF     'Jump to fing JP
E
'*************************************************************************
' JITTER
'*************************************************************************
'=========================================================================
'CALL JL	Jitter Bulk Lift (PAxis) 
'=========================================================================
LB JL
 BR GP, BF=0 	'pause
 BR GP, BF=1	'pause
 BR GJ, BF=2	'Need Pills
 BR GJ, BF=3	'See Pills
 BR GZ, BF=4	'Close
 BR GY, BF=9	'Closed awaiting next command
 BR JL, BF<>999
 OL 0
 H 1
 OL 1
 H 2
 OL 0
 BF=0
 LF=0
 LJ=0
 BR ZZ
E
'=========================================================================
'SUB GF		Find Jitter Point JP  
'=========================================================================
LB GF
 BR JL,BF=999
 BR GZ,BF=4		'jump to close
 LF=1		'Set Finding JP
 LJ=1		'Need JP
 OL 1		'Move to Rest
 R=JN
 CL P1 
 CL ME		'move to min location to start
 CL P2
 R=JM		'try to move to max location 
 CL ME
 JP=P
 LJ=2		'Have JP
 FS=FF		'set Farme start to Current Frame
 FZ=FF		'set Farme start to Current Frame	
 BR JL
E
'=========================================================================
'SUB GP		Jitter Lift Pause  
'=========================================================================
LB GP
 OL 1
 H 1
 OL 0
 FS=FF		'set Farme start to Current Frame
 FZ=FF		'set Farme start to Current Frame	
 R=JP
 JT = JP+DB
 CL ME,P>JT 
 CL LS,LF=2
 LF=2
 LJ=0
 BR JL
E
'=========================================================================
'SUB GJ		Jitter Lift Jitter  
'=========================================================================
LB GJ
 BR GF,LF=9		'if closed and then need more
 BR JL,BF=999
 BR GZ,BF=4		'jump to close
 BR GP,BF=1		'jump to pause   
 CL G0,BF=2		'Need 
 CL G1,BF=3		'See
 OL 2
 BR G4,LJ=0  		'Some place to start
 BR G4,LJ=2		'At Top need to fall
 BR G4,LJ=4		'At Top need to fall
 BR G5,LJ=3		'At Bottom need to raise
 BR JL
E
'=========================================================================
'SUB G0		Jitter Lift Check the Need  
'=========================================================================
LB G0
 FP=FF-FZ
 BR G2, FP>FB		'Cycle Ramp
 FP=FF-FS
 CL G3, FP>FA	 	'Open Door next step
 LF=3			'Jitter for need 
 RT
E
'=========================================================================
'SUB G1		Jitter Lift reset when see  
'=========================================================================
LB G1
 CL G8,LF<>4	'Check to decrease 
 FS=FF		'Reset Counter
 FZ=FF		'Reset Counter
 LF=4		'Jitter See
 RT
E

'=========================================================================
'SUB G8		Jitter Lift Do not decrease every SEE 
'=========================================================================
LB G8
 FP=FF-FZ
 CL G7,FP>=FA	'decrease door 
 RT
E

'=========================================================================
'SUB G7		Jitter Lift See pills decrease door 
'=========================================================================
LB G7
 OL 9
 H 2
 OL 0
 RT
E
'=========================================================================
'SUB G2		Jitter Lift Cycle the Ramp
'=========================================================================
LB G2
 PD=JM - JP	
 CL AB,PD<0	'insure positive
 BR G9, PD<DB
 OL 9		'close the door and decrease  
 H 2
 OL 0
 LJ=5		'Cycling Ramp
 R=JN
 CL P1
 CL ME
 JP=JN
 LJ=1 		'Need to FInd JP
 FZ=FF
 FS=FF
 BR GF
LB G9
 LJ=2		'Have JP
 FS=FF		'set Farme start to Current Frame
 FZ=FF		'set Farme start to Current Frame	
 BR JL 
E

'=========================================================================
'SUB G3		Jitter Lift Send Step the Door  
'=========================================================================
LB G3
 OL 7		'Jitter with Increase
 H 2		'Hold to ensure it gets there
 OL 0 
 FS=FF
 RT
E
'=========================================================================
'SUB G4		Jitter Lift Drop 
'=========================================================================
LB G4
 CL P1
 R=JP-JJ
 CL G6, R<JN
 CL ME
 LJ=3
 BR JL
E
'=========================================================================
'SUB G5		Jitter Lift Raise 
'=========================================================================
LB G5
 CL P3
 R=JP
 CL ME
 LJ=4
 BR JL
E
'=========================================================================
'SUB G6		Jitter Lift Set Min 
'=========================================================================
LB G6
 R=JN
 RT
E
'=========================================================================
'SUB GZ		Jitter Lift Close (Not door Lift min pos)
'=========================================================================
LB GZ
 BR JL,BF<>4	' incase command changes 
 LF=8
 OL 13		'make bulk door reset
 H 2
 OL 0
 R=JN
 CL P1 
 CL ME		'move to min location to start
 PD=P-JN	'Det difference between current position and targe
 CL AB,PD<0	'insure positive
 BR GZ,PD>DB	'Keep Trying
 BF=9
 LF=9
 LJ=0
 BR JL
E
'=========================================================================
'SUB GY		Jitter Lift Closed Waiting  
'=========================================================================
LB GY
 OL 1	 
 H 2
 OL 0
 H 2
 BR JL
E
'=========================================================================
'CALL JD	Jitter Bulk Door (OAxis) 
'=========================================================================
LB JD
 CL MC
 CL P1
 TI 1,KT	'Set trip on IO 1 to KT   
 TE=1		'Trip enabled 
 JP=JJ		'Set JP = to Inital Drop JJ
 DF=0 
 DJ=0
 R2=0
LB KL
 BR KZ,DF=999
 BR KU,I2=1	'Jitter
 BR KV,I2=0	'Rest
 BR KL
E
'=========================================================================
'SUB KV		Bulk Door Ensure at rest position Close
'=========================================================================
LB KV
 BR KZ,DF=999
 R=BM
 PD=P-BM	'Det difference between current position and targe
 CL AB,PD<0	'insure positive
 CL ME,PD>DB
 CL KP,R2>KR
 IC R2
 DJ=2
 BR KL
E

'=========================================================================
'SUB KP		Bulk Door Pause on Opening 
'=========================================================================
LB KP
 H KQ
 R2=0
 RT
E


'=========================================================================
'SUB KU		Bulk Door Do the Jitter 
'=========================================================================
LB KU
 BR KZ,DF=999
 BR KV,DJ=0	'try to Close
 BR KV,DJ=1  	'try to Close
 BR KW,DJ=2	'Try to Open
 BR KL
E
'=========================================================================
'SUB KW		Bulk Door Det direction Open
'=========================================================================
LB KW
 BR KZ,DF=999
 R=JP
 CL KX, R>JM
 CL KY, R<JN
 CL ME
 H JW
 DJ=1
 BR KL
E
'=========================================================================
'SUB KX		Bulk Door keep R below max
'=========================================================================
LB KX
 R = JM
 RT
E
'=========================================================================
'SUB KY		Bulk Door keep R above min 
'=========================================================================
LB KY
 R=JN
 RT
E
'=========================================================================
'SUB KT		Bulk Door Input Trip
'=========================================================================
LB KT
 KF = IL	'Set Input Lower to KF
 CL KC, KF & 4 = 4	'increase
 CL KD, KF & 8 = 8 	'decrease
 CL KE, KF & 12 = 12	'reset
 KF=0
 H 2	'let the motor clear
 TE=1
 RT
E
'=========================================================================
'SUB KC		Bulk Door Input Trip INCREASE
'=========================================================================
LB KC
 JP = JP+JS
 RT
E
'=========================================================================
'SUB KD		Bulk Door Input Trip DECREASE
'=========================================================================
LB KD
 R3 = JS/2
 CL KM,JP>2000
 CL KN,JP>4000
 CL KO,JP>6000
 JP = JP-R3
 RT
E
'=========================================================================
'SUB KM		Bulk Door Input Trip DECREASE (60%)
'=========================================================================
LB KM
 R3=JS*6/10
 RT
E
'=========================================================================
'SUB KN		Bulk Door Input Trip DECREASE (70%)
'=========================================================================
LB KN
 R3=JS*7/10
 RT
E
'=========================================================================
'SUB KO		Bulk Door Input Trip DECREASE (to 4000)
'=========================================================================
LB KO
 R3=JP-4000
 RT
E

'=========================================================================
'SUB KE		Bulk Door Input Trip RESET
'=========================================================================
LB KE
 JP = JJ
 RT
E
'=========================================================================
'SUB KZ		Bulk Door Stop
'=========================================================================
LB KZ
 R=BC
 CL ME
 BR ZZ
E


'*************************************************************************
' MOTION Decisions
'*************************************************************************
'=========================================================================
'CALL DA	Dosis Move ABS (MA R) 
'=========================================================================
LB DA
 CL MC
 DD=1
 BR ML 
E
'=========================================================================
'CALL DM	Dosis Move Relative (MA P+R)
'=========================================================================
LB DM
 CL MC
 DD=2
 BR ML
E
'=========================================================================
'CALL DS	Dosis Speed (SL R) 
'=========================================================================
LB DS
 CL MC
 P=0
 R=0
 T=0
 DD=3
 BR ML
E
'=========================================================================
'CALL DZ	Dosis Loops Close 
'=========================================================================
LB DZ
 DD=0
 BR MZ
E
'=========================================================================
'SUB MC		Motion CleanUP  
'=========================================================================
LB MC
 ER 0
 ST 0
 OE EA
 DE=1		'Ensure Power is on
 SL 0		'Stop Just in-case
 H
 CL P1		'set  motion profile
 CL AB,PD<0	'insure positive
 RE=0		'Clear Rotor Lock
 R=P 		'init R
 T=R		'init T
 PZ=P
 ET=0		'Clear Error Track
 DF=0
 LF=0
 BF=0
 FF=0
 RT
E
'=========================================================================
'SUB ML		Motion Loop  
'=========================================================================
LB ML
 ET=0
 BR MZ,DD=0 	'Loop Closed jump out
 CL MI,R<>T 	'Need to apply motion 
 BR ML,ET=1	'Had an error try to get moving again
 CL LS,DD=3	'Sleep if in slew mode
 CL AB,PD<0	'insure positive No issues to handle/let system rest
 BR ML,DD=3	'In Slew do not need to check DeadBand
 CL MB,MV=0	'Motion Stop det action check DeadBand
 BR ML
E
'=========================================================================
'SUB MI		Motion Decisions    
'=========================================================================
LB MI
 ET=0		'Clear Error Track
 CL M1,DD=1
 CL M2,DD=2
 CL M3,DD=3
 RT
E
'=========================================================================
'SUB MB		Motion Stop check Band    
'=========================================================================
LB MB
  PD=P-T	'Det difference between current position and targe
  CL AB,PD<0	'insure positive
  CL LS,PD<=DB 	'Good Stop let system rest 
  CL MX,PD>DB 	'Not at target try again.  
 RT
E
'=========================================================================
'SUB MX		Motion retry not at location   
'=========================================================================
LB MX
 T=P		'Allow system to retry
 RT
E
'=========================================================================
'SUB MZ		Motion Close   
'=========================================================================
LB MZ
 SL 0
 H
 CL P1
 BR ZZ
E
'=========================================================================
'SUB ME		Motion Execute (complete /Rotor Lock)   
'=========================================================================
LB ME
 ET=0
 CL M1,R<>T 	'Need to apply motion 
 H
 BR ME,ET=1	'Had an error try to get moving again
 PD=P-T		'Det difference between current position and targe
 CL AB,PD<0	'insure positive
 BR MJ, RE=1
 BR ME,PD>DB 
LB MJ
 RT
E
'=========================================================================
'SUB M1		Issue MA (ABS R)
'=========================================================================
LB M1
 T=R
 RE=0
 ET=0
 ER 0
 ST 0
 MA R
 H 1
 RT
E
'=========================================================================
'SUB M2		Issue MA (with reletative R)
'=========================================================================
LB M2
 R=R+P
 T=R
 RE=0
 ET=0
 ER 0
 ST 0
 MA R
 H 1
 RT
E
'=========================================================================
'SUB M3		Issue SL 
'=========================================================================
LB M3
 T=R
 RE=0
 ER 0
 ST 0
 SL R
 H 1
 RT
E
'*************************************************************************
' HOMING SUBS
'*************************************************************************
'=========================================================================
'CALL HH	Homing to Hard Stop 
'=========================================================================
LB HH		'one time set up for each EX HH
 OE EA
 DE 1		'Ensure Power On 
 SL 0
 H
 P=0
 CL P0		'set homing motiSpeed (SL R)on profile
 HE=-1
 RE=0
 T=0
 R=HL*DR
 PZ=P
 ET=0		'Clear Error Track
 BR H0
E
'=========================================================================
'SUB H0		Homing Hard Stop Loop 
'=========================================================================
LB H0		'Homing loop 
  CL M1,R<>T
  H		'Hold for completion or error
  BR HY,RE=1
  PD=P-R	'Det difference between current position and 
		'Max Homing limit
  CL AB,PD<0	'insure positive
  CL HX,PD<=DB	'At Max Homing limit  
  BR H0		'Keep trying until limit or Rotor Lock
E
'=========================================================================
'SUB HY		Homing Complete Good 
'=========================================================================
LB HY
 HE=0		'Homing Passed NO Homing Error
 P=0
 BR HZ
E
'=========================================================================
'SUB HX		Homing Complete Failed 
'=========================================================================
LB HX
 HE=1		'Homing Failed Homing Error
 BR HZ
E
'=========================================================================
'SUB HZ		Homing Finish (STOP EXIT)
'=========================================================================
LB HZ
 CL P1		'set motion profile 1
 SL 0
 H
 OE 0		'Turn off On Error
 OL 0		'Turn off all outputs
 TE 0		'Turn off Trip
E
'*************************************************************************
' GENERAL SUBS
'*************************************************************************
'=========================================================================
'SUB AB		ABS on PD Make Delta Positive 
'=========================================================================
LB AB
 PD=PD*-1
 RT
E
'=========================================================================
'SUB LS		LOOP SLEEP 
'=========================================================================
LB LS
 H 2
 RT
E
'=========================================================================
'SUB ZZ		STOP PG
'=========================================================================
LB ZZ
 SL 0
 H
 OE 0		'Turn off On Error
 OL 0		'Turn off all outputs
 TE 0		'Turn off Trip
 DE 0		'power off
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
'CALL P1 	Set Motion Profile 1
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
'CALL P2 	Set Motion Profile 2
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
'=========================================================================
'CALL P3 Set Motion Profile 3
'=========================================================================
LB P3
 A = A3
 D = B3
 VM = V3
 VI = W3
 SF = F3
 X = X3
 DB = Z3
 RC = J3
 HC = K3
 RT
E
'////////////////////////////  PROGRAM END ///////////////////////////////////
PG 
S






D1=1
D2=1
D3=1
D4=1
S1=0,1,0
S2=0,1,0
S3=0,1,0
S4=0,1,0

JM=9000		'Jitter Max Limit
JN=1000		'Jitter Min Limit
JJ=1250		'Jitter Inital  Value
JS=500		'Jitter  Step Value
BZ=500		'Bulk Door Close
BM=750
KR=2		'Number of Door jitters to cycle before pause
KQ=90  		'mSec to wait for bulk door pause in opening


'Move Profiles (HOMING)
A0=63000    		'Acceleration
B0=1200000        	'Deceleration
V0=8192           	'V Max
W0=100            	'V Initial
F0=24            	'Following Error (SF)
X0=24            	'Rotor Stall
Z0=5            	'Dead Band
J0=25             	'Run Current
K0=25		 	'Hold Current

'Move Profiles (MOTION 1) (open/close/drop)
A1=600000        	'Acceleration
B1=1200000        	'Deceleration
V1=20480          	'V Max
W1=100            	'V Initial
F1=204            	'Following Error (SF)
X1=96            	'Rotor Stall
Z1=5            	'Dead Band
J1=100        	 	'Run Currente
K1=5			'Hold Current

' Dosis software 1.7 expects EM=1 and CK=0
CK=0
EM=1

S
