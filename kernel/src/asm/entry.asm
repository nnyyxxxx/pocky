[BITS 64]

extern kernel_main

section .text
global _start

_start:
    mov rsp, stack_top

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