[BITS 64]

section .text
global switch_context

switch_context:
    test rdi, rdi
    jz .load_new

    mov [rdi + 0x00], rax
    mov [rdi + 0x08], rbx
    mov [rdi + 0x10], rcx
    mov [rdi + 0x18], rdx
    mov [rdi + 0x20], rsi
    mov [rdi + 0x28], rdi
    mov [rdi + 0x30], rbp
    mov [rdi + 0x38], rsp
    mov [rdi + 0x40], r8
    mov [rdi + 0x48], r9
    mov [rdi + 0x50], r10
    mov [rdi + 0x58], r11
    mov [rdi + 0x60], r12
    mov [rdi + 0x68], r13
    mov [rdi + 0x70], r14
    mov [rdi + 0x78], r15

    mov rax, [rsp]
    mov [rdi + 0x80], rax

    pushfq
    pop rax
    mov [rdi + 0x88], rax

    mov [rdi + 0x90], cs
    mov [rdi + 0x98], ss

.load_new:
    mov rax, [rsi + 0x00]
    mov rbx, [rsi + 0x08]
    mov rcx, [rsi + 0x10]
    mov rdx, [rsi + 0x18]
    mov rdi, [rsi + 0x28]
    mov rbp, [rsi + 0x30]
    mov r8,  [rsi + 0x40]
    mov r9,  [rsi + 0x48]
    mov r10, [rsi + 0x50]
    mov r11, [rsi + 0x58]
    mov r12, [rsi + 0x60]
    mov r13, [rsi + 0x68]
    mov r14, [rsi + 0x70]
    mov r15, [rsi + 0x78]

    mov ax, [rsi + 0x90]
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ax, [rsi + 0x98]
    mov ss, ax

    mov rax, [rsi + 0x88]
    push rax
    popfq

    mov rsp, [rsi + 0x38]
    mov rax, [rsi + 0x80]
    push rax

    mov rsi, [rsi + 0x20]

    ret

section .note.GNU-stack noalloc noexec nowrite progbits