ROOT CAUSE
The exported Prepare macro depended on a shared Global Library sender and sent two commands (12 then 10) inside one macro. The second command was left pending because the macro did not robustly wait for controller Last Accepted Seq (4x130) to reach the first command sequence before sending the second command. Diagnoser showed LW1423=10, LW1422 ahead of LW1425, HMI Ready To Start=0, target/tare not applied.

FIX
The new M04_Prepare_Cylinder_V1.ebm is self-contained:
1) Calculates KG/amount.
2) Sends command 12.
3) Waits until 4x130 accepts command 12.
4) Reads actual tare.
5) Writes mode and HMI preset registers 144/146/148/150.
6) Sends command 10.
7) Waits until 4x130 accepts command 10.

IMPORT
Import these EBM files and replace matching macros:
0_M00_Operator_Status_UPdate_V1.ebm
1_M01_Mode_ByKG_V1.ebm
2_M02_Mode_ByAmount_V1.ebm
4_M04_Prepare_Cylinder_V1.ebm
30_M30_Start_Filling_V1.ebm
31_M31_Stop_Filling_V1.ebm
32_M32_Service_Reset_Clear_V1.ebm
33_M33_Next_Cylinder_V1.ebm

Important: These new button macros do not need the Global Library LPG_SendPendingCommand_V1 function. Leave it or ignore it.
