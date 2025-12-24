global printc
printc:
	push bp
	mov bp,sp
	
	mov ah,0x0E
	mov al,[bp+6] ; I'll assume the character to print is here at [bp+6] for now
	int 0x10
	
	pop bp
	ret
