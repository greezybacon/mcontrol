#include "mdrive_base.mxt"

OL 0

D1=1
D2=1
D3=1
D4=1
S1=0,0,0
S2=0,0,0
S3=0,0,0
S4=0,0,0


'Move Profile P0 Homing
A0 = 63000    		'Acceleration
B0 = 1200000        	'Deceleration
V0 = 8192           	'V Max
W0 = 100            	'V Initial
F0 = 24            	'Following Error (SF)
X0 = 24            	'Rotor Stall
Z0 = 5           	'Dead Band
J0 = 25             	'Run Current
K0 = 25			'Hold Current
'
'Move Profile P1 Moving Ramp 
A1 = 800000        	'Acceleration
B1 = 800000        	'Deceleration
V1 = 72000         	'V Max
W1 = 100            	'V Initial
F1 = 205            	'Following Error (SF)
X1 = 102            	'Rotor Stall
Z1 = 5            	'Dead Band
J1 = 75        		'Run Currente
K1 = 5			'Hold Current

' Dosis software 1.7 expects EM=1 and CK=0
CK=0
EM=1

S
