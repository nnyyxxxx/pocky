[BITS 64]

extern mouse_callback

section .text
global mouse_handler

mouse_handler:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    call mouse_callback

    mov al, 0x20
    mov dx, 0xA0
    out dx, al

    mov dx, 0x20
    out dx, al

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    iretq