BITS 16
ORG 0x7C00
TOTAL_SIZE equ 20

XOR AX,AX
MOV DS,AX
MOV ES,AX
MOV SS,AX
MOV SP,0x7C00

MOV AH,0x42
MOV SI,boot_dap
INT 0x13
JC $ ; this error handling is terrible, i'll fix it later

CLI

IN AL,0x92
OR AL,2
OUT 0x92,AL

LGDT [gdt_desc]

MOV EAX,CR0
OR EAX,1
MOV CR0,EAX

JMP CODE_SEG:0x7E00

boot_dap:
db 0x10
db 0x00
dw TOTAL_SIZE-1 ; sectors to read
dw 0x0000
dw 0x1000
dq 1

times 0x50-($-$$) db 0 ; gdt will start at 0x7D00
gdt_table:
dq 0 ; dummy entry
gdt_code:
dw 0xFFFF ; limit low
dw 0x0000 ; base low
db 0x00 ; base mid
db 10011010b ; ACCESS: present, ring 0, OS segment, executable, non-conforming, readable
db 11001111b ; paged, 32-bit, not 64-bit, 0xF limit high
db 0x00 ; base high
gdt_data:
dw 0xFFFF
dw 0x0000
db 0x00
db 10010010b ; ACCESS: present, ring 0, OS segment, data, grows up, writeable
db 11001111b
db 0x00
gdt_tss:
dw tss_end - tss_start - 1
dw tss_start
db 0x00
db 10001001b
db 0x00
db 0x00

times 3 dq 0

gdt_desc:
dw gdt_desc - gdt_table - 1
dd gdt_table

tss_start:
times 104 db 0
tss_end:

CODE_SEG equ gdt_code - gdt_table
DATA_SEG equ gdt_data - gdt_table
TSS_SEG equ gdt_tss - gdt_table


times 0x1BE-($-$$) db 0

; partition entry

db 0x80
db 0x01,0x01,0x00
db 0x0B
db 0xFE,0xFF,0xFF
dd 1
dd TOTAL_SIZE ; adjust to resulting size

times 510-($-$$) db 0
dw 0xAA55