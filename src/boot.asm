bits 16
org 0x7C00

; This sets up all of the memory segments
; For now, we're using 16-bit mode for simplicity
xor ax,ax
mov ds,ax
mov es,ax
mov ss,ax

; This sets up the stack and saves the boot drive number
mov sp,0x7DFF
mov [0xFFFF],dl

; This loads the code from main.c into memory so we can run it
mov ah,0x42
lea si,[dap]
int 0x13
jc err
jmp 0x0000:0x1000

	err:
	; Error handling goes here
	mov dl,ah
	lea si,[errmsg]
	mov ah,0x0E
	err_stringloop:
		lodsb
		cmp al,0
		je err_stringloopend
		int 0x10
		jmp err_stringloop
	err_stringloopend:
	mov al,dl
	int 0x10
	jmp $
	

dap:
	; This contains the command to load main.c into memory
	db 16
	db 0
	dw 1 ; Put the resulting file size, divided by 512, minus 1 here to make sure all sectors get loaded properly
	dw 0x1000
	dw 0x0000
	dq 1

errmsg: db "Failed to load.",0x0A,0x0D,"Search for a BIOS ASCII table online and find this character: ",0

; The MBR partition table is here. You typically don't need to touch this
times 446-($-$$) db 0
db 0x80
db 0x01,0x01,0x00
db 0x83
db 0xFE,0xFF,0xFF
dd 1
dd 10

times 48 db 0
db 0x55,0xAA
