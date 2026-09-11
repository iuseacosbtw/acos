section .text
BITS 32
CODE_SEG equ 0x08
DATA_SEG equ 0x10
TSS_SEG equ 0x18
TOTAL_SIZE equ 20
global _start
_start:

MOV AX,DATA_SEG
MOV DS,AX
MOV ES,AX
MOV FS,AX
MOV GS,AX
MOV SS,AX

MOV ESP,stack_high
MOV EBX,0x7D00
MOV [EBX+4],ESP
MOV [EBX+8],AX

MOV AX,TSS_SEG
LTR AX

JMP _main


; ================================ -INTERRUPT DESCRIPTOR TABLE- ================================

int_pointers: ; these are the interrupt pointers
%assign int_i 0
%rep 8
	PUSH DWORD int_i
	PUSH EAX
	MOV EAX,int_generic
	JMP EAX
	%assign int_i int_i + 1
%endrep
	PUSH DWORD int_i
	PUSH EAX
	MOV EAX,int_argument
	JMP EAX
	%assign int_i int_i + 1

	PUSH DWORD int_i
	PUSH EAX
	MOV EAX,int_generic
	JMP EAX
	%assign int_i int_i + 1
%rep 5
	PUSH DWORD int_i
	PUSH EAX
	MOV EAX,int_argument
	JMP EAX
	%assign int_i int_i + 1
%endrep
%rep 2
	PUSH DWORD int_i
	PUSH EAX
	MOV EAX,int_generic
	JMP EAX
	%assign int_i int_i + 1
%endrep
	PUSH DWORD int_i
	PUSH EAX
	MOV EAX,int_argument
	JMP EAX
	%assign int_i int_i + 1
%rep 14
	PUSH DWORD int_i
	PUSH EAX
	MOV EAX,int_generic
	JMP EAX
	%assign int_i int_i + 1
%endrep
%rep 8
	PUSHAD
	PUSH DWORD int_i
	MOV EAX,int_irq1
	JMP EAX
	%assign int_i int_i + 1
%endrep
%rep 8
	PUSH DWORD int_i
	PUSH EAX
	MOV EAX,int_irq2
	JMP EAX
	%assign int_i int_i + 1
%endrep
	PUSH DWORD int_i
	PUSH EAX
	MOV EAX,int_oshandler
	JMP EAX
int_pointers_end:

load_idt: ; properly loads the interrupt pointers
	PUSH ESI
	PUSH EDI
	PUSH ECX
	
	MOV ESI,int_pointers
	MOV EDI,0x1000
	MOV ECX,int_pointers_end-int_pointers
	REP MOVSB
	
	POP ECX
	POP EDI
	POP ESI
	RET

; arg 1=function pointer, arg 2=flags (0x8E for CLI, 0x8F otherwise)
%macro IDTENT 2
	dw (%1) & 0xFFFF
	dw CODE_SEG
	db 0
	db %2
	dw ((%1) >> 16) & 0xFFFF
%endmacro

interrupt_desc_table_pointer:
dw interrupt_desc_table_end - interrupt_desc_table - 1
dd interrupt_desc_table

interrupt_desc_table:
%assign int_i 0

%rep 32+16+1
	IDTENT 0x1000+int_i, 0x8E
	%assign int_i int_i + 10
%endrep
interrupt_desc_table_end:


; ================================ -INTERRUPT HANDLERS- ================================
; These handlers do not save the callee-saved registers like
; normal functions do. Instead, they clean everything
; up and return control to the kernel instead of
; the program.

extern IRQ_handler
int_irq1:
	MOV AL,0x20
	OUT 0x20,AL
	
	POP EDX
	
	PUSH DS
	PUSH ES
	PUSH FS
	PUSH GS
	
	MOV AX,DATA_SEG
	MOV DS,AX
	MOV ES,AX
	
	PUSH ESP
	PUSH EDX
	CALL IRQ_handler
	MOV ESP,EAX
	
	POP GS
	POP FS
	POP ES
	POP DS
	
	MOV [int_irq1_eax],EAX
	MOV [int_irq1_stack],ESP
	POPAD
	MOV EAX,[int_irq1_stack]
	MOV ESP,EAX
	ADD ESP,0x20
	MOV EAX,[int_irq1_eax]
	
	IRET
	int_irq1_stack: dd 0
	int_irq1_eax: dd 0
int_irq2:
	MOV AL,0x20
	OUT 0xA0,AL
	OUT 0x20,AL
	POP EAX
	ADD ESP,4
	IRET

int_argument:
	CLI
	;POP EAX
	;POP EAX
	;POP EDX
	;PUSH EAX
int_generic:
	CLI
	POP EAX
	POP EAX
	PUSH EAX
	CALL clear
	MOV BYTE [G_Printing_TextAttribute],0x8C ; red text
	PUSH panic_str
	CALL prints
	MOV BYTE [G_Printing_TextAttribute],0x0F
	PUSH DWORD int_generic_string
	CALL prints_raw
	CALL printh
	PUSH DWORD int_generic_string2
	CALL prints_raw
	
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
		LOOP int_generic_loop
	
	int_halt:
	HLT
	JMP int_halt
	int_generic_string: db "INTERRUPT 0x",0
	int_generic_string2: db " TRIGGERED AT 0x",0

int_oshandler:
	POP EAX
	ADD ESP,4
	IRET


; kernel functions and global variables
; this is the literal kernel

; we use stdcall here: arguments are pushed right-to-left and the callee cleans up the stack.

; ================================ -PRINTING- ================================
; This section is for printing text on the screen.
; It involves manipulating the cursor and printing strings.
; Not much else to it.

G_Printing_TextCursor: dw 0 ; location of the text cursor
G_Printing_TextAttribute: db 0x0F ; attribute of the next character to be typed

; ================ setcursorpos ================
; Sets the position of the text cursor.
; x must be between 0 and 79.
; y must be between 0 and 24.
; If x and y are not between these limits, they will be assumed to be 0.
; 
; ARGS: x, y
; OUTPUT: void
; CLOBBERS: AX, CL, DX
; ================ setcursorpos ================
global setcursorpos
setcursorpos:
	PUSH EBP
	MOV EBP,ESP
	
	MOV AX,[EBP+12]
	CMP AX,25
	JB setcursorpos_yisvalid
	XOR AX,AX
	setcursorpos_yisvalid:
	MOV DX,[EBP+8]
	CMP DX,80
	JB setcursorpos_xisvalid
	XOR DX,DX
	setcursorpos_xisvalid:
	MOV CL,80
	MUL CL
	ADD AX,DX
	MOV [G_Printing_TextCursor],AX
	
	POP EBP
	RET 8

; ================ getcursorpos ================
; Gets the current position of the text cursor.
; 
; ARGS: void
; OUTPUT: AL=x, DL=y
; CLOBBERS: EAX, CX, EDX
; ================ getcursorpos ================
; NO GLOBAL!!!
getcursorpos:
	XOR EDX,EDX
	MOV EAX,[G_Printing_TextCursor]
	MOV CX,80
	DIV CX
	XCHG EAX,EDX
	
	RET

; ================ blink ================
; Updates the visible location of the cursor.
; Meant to be used internally, by other functions,
; or if you modify G_Printing_TextCursor specifically.
; 
; ARGS: void
; OUTPUT: void
; CLOBBERS: AX, DX
; ================ blink ================
; NO GLOBAL!!!
blink:
	PUSH EBX
	
	MOV DX,0x3D4
	MOV AL,0x0F
	OUT DX,AL
	
	INC DX
	MOV AX,[G_Printing_TextCursor]
	OUT DX,AL
	
	MOVZX EBX,AX
	ADD EBX,0xB8000/2
	MOV AL,DL
	MOV DL,[G_Printing_TextAttribute]
	MOV [EBX*2+1],DL
	MOV DL,AL
	
	DEC DX
	MOV AL,0x0E
	OUT DX,AL
	
	INC DX
	MOV AL,AH
	OUT DX,AL
	
	POP EBX
	RET

; ================ printc ================
; Prints the character onto the screen, handling newlines, backspaces, etc.
; 
; ARGS: char
; OUTPUT: void
; CLOBBERS: EAX, CX, EDX
; ================ printc ================
global printc
printc:
	PUSH EBP
	MOV EBP,ESP
	PUSH EBX
	
	MOVZX EBX,WORD [G_Printing_TextCursor]
	ADD EBX,0xB8000/2 ; we're gonna shift right
	ADD EBX,EBX ; EBX now contains offset
	
	MOV EAX,[EBP+8]
	
	CMP AL,0x20
	JAE printc_not_ctrl_char
	CMP AL,0x0A ; line feed
	JE printc_lf
	CMP AL,0x0D ; carriage return
	JE printc_cr
	CMP AL,0x08 ; backspace
	JE printc_bksp
	JMP printc_null
	printc_not_ctrl_char:
	
	MOV [EBX],AL
	MOV AL,[G_Printing_TextAttribute]
	MOV [EBX+1],AL
	
	INC WORD [G_Printing_TextCursor]
	CMP WORD [G_Printing_TextCursor],2000
	JB printc_cleanup
	
	MOV WORD [G_Printing_TextCursor],0
	
	printc_cleanup:
	CALL blink
	printc_null:
	POP EBX
	POP EBP
	RET 4
	
	printc_lf:
		CALL getcursorpos
		INC EDX
		PUSH EDX
		PUSH DWORD 0
		CALL setcursorpos
		JMP printc_cleanup
	printc_cr:
		CALL getcursorpos
		PUSH EDX
		PUSH DWORD 0
		CALL setcursorpos
		JMP printc_cleanup
	printc_bksp:
		CMP WORD [G_Printing_TextCursor],0
		JE printc_cleanup
		DEC WORD [G_Printing_TextCursor]
		DEC EBX
		MOV AL,[G_Printing_TextAttribute]
		MOV [EBX],AL
		DEC EBX
		MOV BYTE [EBX],0x20
		JMP printc_cleanup

; ================ prints ================
; Prints the string onto the screen, using printc for each character.
; Stops printing once it reaches a 0x00 byte.
; 
; ARGS: char*
; OUTPUT: void
; CLOBBERS: EAX, CX, EDX
; ================ prints ================
global prints
prints:
	PUSH EBP
	MOV EBP,ESP
	PUSH ESI
	
	MOV ESI,[EBP+8]
	CLD
	
	prints_loop:
		CMP BYTE [ESI],0
		JE prints_cleanup
		PUSH DWORD [ESI]
		CALL printc
		INC ESI
		JMP prints_loop
	
	prints_cleanup:
	CALL blink
	POP ESI
	POP EBP
	RET 4

; ================ prints_raw ================
; Prints the string onto the screen, printing each character exactlty as-is.
; Stops printing once it reaches a 0x00 byte.
; 
; ARGS: char*
; OUTPUT: void
; CLOBBERS: AX, DX
; ================ prints_raw ================
global prints_raw
prints_raw:
	PUSH EBP
	MOV EBP,ESP
	PUSH ESI
	PUSH EDI
	
	MOV ESI,[EBP+8]
	MOV DX,[G_Printing_TextCursor]
	
	MOVZX EDI,DX ; same routine as printc
	ADD EDI,0xB8000/2 
	ADD EDI,EDI
	
	MOV AL,[G_Printing_TextAttribute]
	CLD
	
	prints_raw_loop:
		CMP BYTE [ESI],0
		JE prints_raw_cleanup
		MOVSB
		MOV [EDI],AL
		INC EDI
		INC DX
		JMP prints_raw_loop
	
	prints_raw_cleanup:
	MOV [G_Printing_TextCursor],DX
	CALL blink
	POP EDI
	POP ESI
	POP EBP
	RET 4

; ================ printc_raw ================
; Prints the character onto the screen, as-is.
; 
; ARGS: char
; OUTPUT: void
; CLOBBERS: AX, DX
; ================ printc_raw ================
global printc_raw
printc_raw:
	PUSH EBP
	MOV EBP,ESP
	PUSH EBX
	
	MOVZX EBX,WORD [G_Printing_TextCursor]
	ADD EBX,0xB8000/2
	ADD EBX,EBX
	
	MOV EAX,[EBP+8]
	MOV [EBX],AL
	MOV AL,[G_Printing_TextAttribute]
	MOV [EBX+1],AL
	
	INC WORD [G_Printing_TextCursor]
	CALL blink
	
	POP EBX
	POP EBP
	RET 4

; ================ printh ================
; Prints a byte, two hex digits, onto the screen.
; 
; ARGS: char
; OUTPUT: void
; CLOBBERS: AX, DX
; ================ printh ================
global printh
printh:
	PUSH EBP
	MOV EBP,ESP
	PUSH EBX
	
	MOV DX,[G_Printing_TextCursor]
	MOVZX EBX,DX
	ADD EBX,0xB8000/2
	ADD EBX,EBX
	
	MOV AH,[G_Printing_TextAttribute]
	
	MOV AL,[EBP+8] ; first digit
	SHR AL,4
	ADD AL,'0'
	CMP AL,'9'
	JBE printh_1good
	
	ADD AL,'A'-0x3A
	
	printh_1good:
	MOV [EBX],AX
	ADD EBX,2
	
	MOV AL,[EBP+8]
	AND AL,0x0F
	ADD AL,'0'
	CMP AL,'9'
	JBE printh_2good
	
	ADD AL,'A'-0x3A
	
	printh_2good:
	MOV [EBX],AX
	ADD DX,2
	MOV [G_Printing_TextCursor],DX
	
	CALL blink
	POP EBX
	POP EBP
	RET 4

; ================ clear ================
; Clears the screen and resets the cursor.
; 
; ARGS: void
; OUTPUT: void
; CLOBBERS: EAX, ECX, DX
; ================ clear ================
global clear
clear:
	PUSH EDI
	
	XOR EAX,EAX
	MOV [G_Printing_TextCursor],AX
	MOV EDI,0xB8000
	MOV ECX,1000
	REP STOSD
	
	CALL blink
	POP EDI
	RET

; ================================ -KEYBOARD- ================================

G_Keyboard_KeyFlags: db 00000000b ; b0=ctrl 1D, b1=shift 2A, b2=alt 38, b3=caps 3A
G_Keyboard_Scancodes: ; the list of scancodes for the scancode_to_ascii function
	db 0x00, 0x1B, '1', '2', '3', '4', '5', '6'
	db '7', '8', '9', '0', '-', '=', 0x8, 0x9
	db 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i'
	db 'o', 'p', '[', ']', 0xA, 0x0, 'a', 's'
	db 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';'
	db "'", '`', 0x0, '\', 'z', 'x', 'c', 'v'
	db 'b', 'n', 'm', ',', '.', '/', 0x0, 0x0
	db 0x0, ' ', 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	db 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	db 0x1, 0x0, 0x0, 0x2, 0x0, 0x4, 0x0, 0x0
	db 0x3
	times 0x100-($-G_Keyboard_Scancodes) db 0
G_Keyboard_ShiftedScancodes:
	db 0x0, 0x1B, '!', '@', '#', '$', '%', '^'
	db '&', '*', '(', ')', '_', '+', 0x8, 0x9
	db 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I'
	db 'O', 'P', '{', '}', 0xA, 0x0, 'A', 'S'
	db 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':'
	db '"', '~', 0x0, '|', 'Z', 'X', 'C', 'V'
	db 'B', 'N', 'M', '<', '>', '?', 0x0, 0x0
	db 0x0, ' ', 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	db 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	db 0x1, 0x0, 0x0, 0x2, 0x0, 0x4, 0x0, 0x0
	db 0x3
	times 0x100-($-G_Keyboard_ShiftedScancodes) db 0
	

; ================ scancode_to_ascii ================
; Converts a scan code to an ASCII code.
; CONSIDERS THE CURRENT VALUE OF G_Keyboard_KeyFlags
; 
; ARGS: ScanCode
; OUTPUT: AL=AsciiCode
; CLOBBERS: AL
; ================ scancode_to_ascii ================
global scancode_to_ascii
scancode_to_ascii:
	PUSH EBP
	MOV EBP,ESP
	PUSH EBX
	
	MOV AL,[G_Keyboard_KeyFlags]
	TEST AL,00000010b
	JNZ scancode_to_ascii_noshift
	OR AL,0x80
	scancode_to_ascii_noshift:
	TEST AL,00001000b
	JNZ scancode_to_ascii_nocaps
	XOR AL,0x80
	scancode_to_ascii_nocaps:
	
	TEST AL,0x80
	JNZ scancode_to_ascii_shiftflag
	MOV EBX,G_Keyboard_Scancodes
	JMP scancode_to_ascii_noshiftflag
	scancode_to_ascii_shiftflag:
	MOV EBX,G_Keyboard_ShiftedScancodes
	scancode_to_ascii_noshiftflag:
	
	ADD BL,[EBP+8]
	ADC EBX,0
	MOV AL,[EBX]
	
	POP EBX
	POP EBP
	RET 4

; ================ getchar ================
; Gets the next character's ASCII code typed by the user.
; BLOCKS EXECUTION if no character has been typed yet.
; 
; ARGS: void
; OUTPUT: AL=AsciiCode
; CLOBBERS: AX
; ================ getchar ================
global getchar
getchar:
	CALL getscancode
	PUSH EAX
	CALL scancode_to_ascii
	RET

; ================ getscancode ================
; Gets the next character's scan code typed by the user.
; BLOCKS EXECUTION if no character has been typed yet.
; 
; ARGS: void
; OUTPUT: AL=ScanCode
; CLOBBERS: AX
; ================ getscancode ================
global getscancode
getscancode_loop:
	HLT
getscancode:
	IN AL,0x64 ; wait until input buffer is filled
	TEST AL,1
	JZ getscancode_loop
	
	IN AL,0x60
	MOV AH,AL
	AND AH,0x7F
	
	CMP AH,0x1D ; ctrl
	JE getscancode_ctrl
	CMP AH,0x2A ; shift
	JE getscancode_shift
	CMP AH,0x38 ; alt
	JE getscancode_alt
	CMP AH,0x3A ; caps lock
	JE getscancode_caps
	
	TEST AL,0x80 ; make sure it's a key press, not a key release
	JNZ getscancode
	
	RET
	
	getscancode_ctrl:
	SHR AL,7
	XOR AL,00000001b
	AND BYTE [G_Keyboard_KeyFlags],11111110b
	OR [G_Keyboard_KeyFlags],AL
	JMP getscancode
	
	getscancode_shift:
	SHR AL,6
	AND AL,00000010b
	XOR AL,00000010b
	AND BYTE [G_Keyboard_KeyFlags],11111101b
	OR [G_Keyboard_KeyFlags],AL
	JMP getscancode
	
	getscancode_alt:
	SHR AL,5
	AND AL,00000100b
	XOR AL,00000100b
	AND BYTE [G_Keyboard_KeyFlags],11111011b
	OR [G_Keyboard_KeyFlags],AL
	JMP getscancode
	
	getscancode_caps:
	SHR AL,4
	AND AL,00001000b
	XOR AL,00001000b
	XOR [G_Keyboard_KeyFlags],AL ; caps lock toggles unlike the other keys
	
	getscancode_caps_wait: ; handle the LED lightt
		IN AL,0x64
		TEST AL,2
		JNZ getscancode_caps_wait
	MOV AL,0xED
	OUT 0x60,AL
	
	getscancode_caps_wait2:
		IN AL,0x64
		TEST AL,2
		JNZ getscancode_caps_wait2
	MOV AL,[G_Keyboard_KeyFlags]
	AND AL,00001000b
	SHR AL,1
	OUT 0x60,AL
	
	JMP getscancode


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
	
	
	
	POP EBP
	RET 12

; ================ settimer ================
; Programs the hardware timer
; 
; ARGS: Time
; OUTPUT: none
; CLOBBERS: ur mom
; ================ settimer ================
extern settimer
settimer:
	PUSH EBP
	MOV EBP,ESP
	
	MOV AL,0x36
	OUT 0x43,AL
	
	MOV AX,[EBP+8]
	OUT 0x40,AL
	MOV AL,AH
	OUT 0x40,AL
	
	POP EBP
	RET 4


; ================================================================ -=============================- ================================================================
; ================================================================ -=============================- ================================================================
; ================================================================ -THE ALL-CONTROLLING SCHEDULER- ================================================================
; ================================================================ -=============================- ================================================================
; ================================================================ -=============================- ================================================================

; ================ reload_gdt ================
; Reloads the GDT with the user's segment descriptors
; 
; ARGS: proc_gdt*
; OUTPUT: void
; CLOBBERS: ECX
; ================ reload_gdt ================
global reload_gdt
reload_gdt:
	PUSH EBP
	MOV EBP,ESP
	PUSH ESI
	PUSH EDI
	
	MOV ESI,[EBP+8]
	MOV EDI,0x7C70
	
	CLD
	MOV ECX,6
	REP MOVSD
	
	LGDT [EDI]
	
	POP EDI
	POP ESI
	POP EBP
	RET 4

; ================ delay ================
; Returns control to the scheduler and
; pauses the program until the time is over.
; 
; ARGS: dword TimeToDelay
; OUTPUT: void
; CLOBBERS: ECX
; ================ delay ================


_main:

; remap the PIC
mov al,0x11
out 0x20,al
out 0xA0,al

mov al,0x20
out 0x21,al
mov al,0x28
out 0xA1,al

mov al,0x04
out 0x21,al
mov al,0x02
out 0xA1,al

mov al,0x01
out 0x21,al
out 0xA1,al

mov al,0x00
out 0x21,al
out 0xA1,al
; this looks different because it's not mine

CALL load_idt
LIDT [interrupt_desc_table_pointer]
STI
; interrupts are now active


; scheduler! scheduler! yayy~!




extern kmain ; this is where assembly retires and C doesn't haha

CALL clear
CALL kmain

MOV ESP,stack_high
CALL clear
MOV BYTE [G_Printing_TextAttribute],0x8C ; red text
PUSH panic_str
CALL prints
MOV BYTE [G_Printing_TextAttribute],0x0F
PUSH err_str
CALL prints_raw

_panic_loop:
CLI
HLT
JMP _panic_loop

panic_str: db "PANIC",0x0A,0
err_str: db "Kernel returned unexpectedly!",0


section .bss
stack_low:
resb 0x4000
stack_high:
