root equ 0xA000

ORG root

dd file_end-filesystem_sizer
dd root
dd root
db 0b01_000_110

filesystem_sizer:

dd root
db "/",0

dd file_end
db "alloc.sys",0

dd file2_end
db "test.ace",0

file_end:
dd 8
dd root
dd file_end
db 0b00_000_000

; fix up the directory reading functions, including resolve_path
; getFileName works!
; ADD PERMISSIONS to the file-reading functions
;
; create a function simply to write a file to disk

dd filesystem_new_root_end
dd root+root-filesystem_new_root_end

file2_end:
dd filesystem_new_root_end-skibidi_file+4
dd root
dd file2_end
db 0b00_000_111

db "ACOS"
skibidi_file:

PUSH DWORD str-skibidi_file
MOV EAX,4
INT 0x30

UD2

XOR EAX,EAX
INT 0x30

JMP $

str: db "Hello Wooorld!",0
filesystem_new_root_end:
