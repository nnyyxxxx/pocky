align 8
gdt64_start:
    dd 0, 0

gdt64_code:
    dw 0
    dw 0
    db 0
    db 10011010b
    db 00100000b
    db 0

gdt64_data:
    dw 0
    dw 0
    db 0
    db 10010010b
    db 00000000b
    db 0

gdt64_end:

gdt64_descriptor:
    dw gdt64_end - gdt64_start - 1
    dq gdt64_start

CODE_SEG equ gdt64_code - gdt64_start
DATA_SEG equ gdt64_data - gdt64_start