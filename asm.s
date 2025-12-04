	.def setASP
	.def setPSP
	.def getPSP
	.def getMSP
	.def setTMPL
	.def pushRegs
	.def popRegs

.thumb
.const

.text

setASP:
	MOV R0, #2
	MSR CONTROL, R0
	ISB
	BX LR

setPSP:
	MSR PSP, R0
	BX LR

getPSP:
	MRS R0, PSP
	BX LR

getMSP:
	MRS R0, MSP
	BX LR

setTMPL:
	MRS R0, CONTROL
	ORR R0, #1
	MSR CONTROL, R0
	ISB
	BX LR

pushRegs:
	MRS R0, PSP 		;current PSP into R0
   	SUB R0, R0, #36 	; Adjust for 9 regs
    STR R4, [R0, #32]
	STR R5, [R0, #28]
	STR R6, [R0, #24]
	STR R7, [R0, #20]
	STR R8, [R0, #16]
	STR R9, [R0, #12]
	STR R10, [R0, #8]
	STR R11, [R0, #4]
    MOVW R1, #0xFFFD
    MOVT R1, #0xFFFF
    STR R1, [R0, #0] 	;LR
    MSR PSP, R0			;Restores PSP
    BX LR

popRegs:
	MRS R0, PSP
	LDR LR,[R0],#4
	LDR R11,[R0],#4
	LDR R10,[R0],#4
	LDR R9,[R0],#4
	LDR R8,[R0],#4
	LDR R7,[R0],#4
	LDR R6,[R0],#4
	LDR R5,[R0],#4
	LDR R4,[R0],#4
	MSR PSP,R0
	BX LR
endm
