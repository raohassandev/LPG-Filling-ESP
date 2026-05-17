WORD LAMP / STATE TEXTS
=======================

Header Fill State
-----------------
Device: MODBUS RTU (Zero-based Addressing)
Tag: Fill State
Type: Word Lamp

Value | Text
0     | IDLE
1     | READY
2     | APPLY SETUP
3     | FILLING
4     | FILLING
5     | SETTLING
6     | COMPLETE
7     | STOPPED
8     | FAULT
9     | SERVICE

Header Alarm
------------
Device: MODBUS RTU (Zero-based Addressing)
Tag: Alarm Code
Type: Word Lamp

Use short text in the header pill. Put full alarm detail on Alarm screen.

Value | Header Text
0     | ALARM OK
1     | E-STOP
2     | NOZZLE
3     | CYLINDER
4     | SCALE ERROR
5     | UNSTABLE
6     | NOT CALIBRATED
7     | OVERFILL
8     | TIMEOUT
9     | NO FLOW
10    | LOG ERROR
11    | OPERATOR STOP
12    | ACTIVE ALARM
13    | OFFLINE

Machine Status Strip
--------------------
Recommended source: Fill State
Alternative: HMI Ready To Start for simple READY/NOT READY strip.

Value | Text
0     | IDLE
1     | READY
2     | APPLY SETUP
3     | FILLING
4     | FILLING
5     | SETTLING
6     | COMPLETE
7     | STOPPED
8     | FAULT
9     | SERVICE

Readiness Bit Lamps
-------------------
EStop OK           : tag EStop OK            0=NOT OK, 1=OK
Cylinder Present   : tag Cylinder Present    0=NOT OK, 1=OK
Nozzle Engaged     : tag Nozzle Engaged      0=NOT OK, 1=OK
Weight Stable      : tag Weight Stable       0=NOT OK, 1=OK
