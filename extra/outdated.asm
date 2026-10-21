; ================================ -HARD DISK- ================================

; ================ readsectors ================
; Reads sectors from the HDD and loads them into memory.
; Sets the carry flag if there was an error,
; and puts the error code in AL
; 
; ARGS: StartSector, SectorsToRead, MemoryBuffer
; OUTPUT: AL=ErrorCode
; CLOBBERS: EAX, ECX, DX
; ================ readsectors ================
readsectors:
	PUSH EBP
	MOV EBP,ESP
	PUSH EDI
	
	MOVZX ECX,BYTE [EBP+12] ; set number of sectors
	MOV AL,CL
	MOV DX,0x1F2
	OUT DX,AL
	
	MOV EAX,[EBP+16] ; set starting sector 0-7
	INC DX
	OUT DX,AL
	
	SHR EAX,8 ; 8-15
	INC DX
	OUT DX,AL
	
	SHR EAX,8 ; 16-23
	INC DX
	OUT DX,AL
	
	SHR EAX,8; 24-27 and disk select
	INC DX
	AND AL,0x0F
	OR AL,0xE0
	OUT DX,AL
	
	INC DX ; read command
	MOV AL,0x20
	OUT DX,AL
	
	MOV EDI,[EBP+8]
	
	readsectors_loop:
		IN AL,DX
		TEST AL,8
		JZ readsectors_loop
		TEST AL,0x21
		JNZ readsectors_error
		
		MOV DX,0x1F0
		PUSH ECX
		MOV ECX,0x100
		REP INSW ; read 512 bytes (1 sector)
		POP ECX
		
		MOV DX,0x1F7 ; set up for next sector
		LOOP readsectors_loop
	
	CLC
	readsectors_cleanup:
	POP EDI
	POP EBP
	RET 12
	
	readsectors_error:
	MOV DX,0x1F1
	IN AL,DX
	STC
	JMP readsectors_cleanup