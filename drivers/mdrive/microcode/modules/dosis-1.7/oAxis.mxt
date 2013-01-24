
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
