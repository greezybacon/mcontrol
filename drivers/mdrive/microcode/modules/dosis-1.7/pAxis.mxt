#include "mdrive_base.mxt"

OL 0

S1=16,1,0
S2=16,1,0
S3=16,1,0
S4=16,1,0

JM=25500	'Jitter Max Limit
JN=12500	'Jitter Min Limit
JJ=1024		'Jitter Inital  Value
JS=512		'Jitter  Step Value
LO=12200	'Access Door (LIFT) Open Position
LC=50		'Access Door (LIFT) Close Position
FA=15	'Frame Transition A (Door)
FB=90	'Frame Transition B (Ramp)




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
A1=1200000        	'Acceleration
B1=1800000        	'Deceleration
V1=30720          	'V Max
W1=100            	'V Initial
F1=410            	'Following Error (SF)
X1=204            	'Rotor Stall
Z1=5            	'Dead Band
J1=100        	 	'Run Currente
K1=25			'Hold Current


'Move Profiles  (MOTION 2) (find JP)
A2=600000    	 	'Acceleration
B2=1200000         	'Deceleration
V2=8192          	'V Max
W2=100            	'V Initial
F2=24           	'Following Error (SF)
X2=24           	'Rotor Stall
Z2=5           		'Dead Band
J2=50            	'Run Current
K2=50		 	'Hold Current

'Move Profiles  (MOTION 3) (Lifting)
A3 = 600000     	'Acceleration
B3 = 4000000         	'Deceleration
V3 = 32000          	'V Max
W3 = 100            	'V Initial
F3 = 100           	'Following Error (SF)
X3 = 48            	'Rotor Stall
Z3 = 5           	'Dead Band
J3 = 100            	'Run Current
K3 = 50		 	'Hold Current

' Dosis software 1.7 expects EM=1 and CK=0
CK=0
EM=1

S
