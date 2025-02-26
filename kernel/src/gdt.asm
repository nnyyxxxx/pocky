section .text
global load_gdt

load_gdt:
    mov rax, rsp

    lgdt [rdi]

    mov dx, 0x10
    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx
    mov ss, dx

    push 0x08
    lea rcx, [rel .reload_cs]
    push rcx
    retfq
.reload_cs:
    mov rsp, rax
    ret