section .text

bits 64

global read_cr4
global write_cr4
global write_cr3
global read_cr0
global write_cr0
global read_efer
global write_efer
global flush_tlb

read_cr4:
    push rbp
    mov rbp, rsp
    mov rax, cr4
    pop rbp
    ret

write_cr4:
    push rbp
    mov rbp, rsp
    mov cr4, rdi
    pop rbp
    ret

write_cr3:
    push rbp
    mov rbp, rsp
    mov cr3, rdi
    pop rbp
    ret

read_cr0:
    push rbp
    mov rbp, rsp
    mov rax, cr0
    pop rbp
    ret

write_cr0:
    push rbp
    mov rbp, rsp
    mov cr0, rdi
    pop rbp
    ret

read_efer:
    push rbp
    mov rbp, rsp
    push rcx
    push rdx

    mov rcx, 0xC0000080
    rdmsr

    shl rdx, 32
    mov eax, eax
    or rax, rdx

    pop rdx
    pop rcx
    pop rbp
    ret

write_efer:
    push rbp
    mov rbp, rsp
    push rcx
    push rdx

    mov rcx, 0xC0000080
    mov rax, rdi
    mov rdx, rdi
    shr rdx, 32
    wrmsr

    pop rdx
    pop rcx
    pop rbp
    ret

flush_tlb:
    push rbp
    mov rbp, rsp
    mov rax, cr3
    mov cr3, rax
    pop rbp
    ret

section .note.GNU-stack noalloc noexec nowrite progbits