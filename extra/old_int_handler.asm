int_generic:
	MOV AX,DATA_SEG
	MOV DS,AX
	MOV ES,AX

	POP EAX
	POP EAX
	PUSH EAX

	PUSH DWORD int_generic_string
	CALL panic
	ADD ESP,4
	CALL printh
	ADD ESP,4
	PUSH DWORD int_generic_string2
	CALL prints_raw
	ADD ESP,4

	POP EDX
	PUSH EDX

	MOV AL,DL
	PUSH EAX

	MOV AL,DH
	PUSH EAX

	MOV EAX,EDX
	SHR EAX,16
	PUSH EAX

	MOV EAX,EDX
	SHR EAX,24
	PUSH EAX

	MOV ECX,4
	int_generic_loop:
		CALL printh
		ADD ESP,4
		LOOP int_generic_loop

	int_halt:
	HLT
	JMP int_halt
	int_generic_string: db "INTERRUPT 0x",0
	int_generic_string2: db " TRIGGERED AT 0x",0
