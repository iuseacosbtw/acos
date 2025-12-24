bits 16
org 0x7C00

start:
	mov ax,0x0E41
	xor bx,bx
	int 0x10
	jmp $

times 510-($-$$) db 0
db 0x55,0xAA
