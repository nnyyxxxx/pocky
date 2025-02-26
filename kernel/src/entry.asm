[BITS 32]

extern kernel_main

section .text
global _start

_start:
    mov esp, stack_top

    cld

    call kernel_main

    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 65536
stack_top: