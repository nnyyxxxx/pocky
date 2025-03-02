[BITS 32]

extern kernel_main
extern init_global_constructors
extern init_multiboot

section .multiboot_header
align 8
header_start:
    dd 0xE85250D6
    dd 0
    dd header_end - header_start
    dd -(0xE85250D6 + 0 + (header_end - header_start))

    dw 0
    dw 0
    dd 8
header_end:

%define PAGE_PRESENT    (1 << 0)
%define PAGE_WRITE      (1 << 1)
%define PAGE_1G         (1 << 7)
%define CR4_PAE         (1 << 5)
%define CR4_PSE         (1 << 4)
%define MSR_EFER        0xC0000080
%define EFER_LME        (1 << 8)
%define CR0_PG          (1 << 31)
%define CR0_PE          (1 << 0)
%define CR0_MP          (1 << 1)

section .data
align 8
no_mb_msg db "ERROR: Not loaded by multiboot-compliant bootloader!", 0
magic_err_msg db "ERROR: Wrong magic value!", 0
mb_ok_msg db "Multiboot2 found, initializing...", 0
paging_msg db "Setting up paging for long mode...", 0
long_mode_msg db "Entering 64-bit mode...", 0
hex_chars db "0123456789ABCDEF", 0

multiboot_magic dd 0
multiboot_info dd 0

section .bss
align 16
global p4_table
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096

align 16
stack_bottom:
    resb 64 * 1024
stack_top:

section .text
global _start

print_string_32:
    pusha
    mov ebx, 0xB8000
    xor ecx, ecx
    mov ah, 0x4F

.loop:
    lodsb
    test al, al
    jz .done
    mov [ebx + ecx*2], ax
    inc ecx
    jmp .loop

.done:
    popa
    ret

_start:
    mov [multiboot_magic], eax
    mov [multiboot_info], ebx

    mov esp, stack_top

    cmp eax, 0x36d76289
    jne .no_multiboot

    mov esi, mb_ok_msg
    call print_string_32

    call setup_paging

    call enter_long_mode

    lgdt [gdt64.pointer]

    jmp gdt64.code:long_mode_start

.no_multiboot:
    mov esi, no_mb_msg
    call print_string_32
    jmp hang_32

setup_paging:
    mov esi, paging_msg
    call print_string_32

    mov edi, p4_table
    xor eax, eax
    mov ecx, 4096*3/4
    rep stosd

    mov eax, p3_table
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [p4_table], eax

    mov eax, p2_table
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [p3_table], eax

    mov ecx, 0
.map_p2:
    mov eax, ecx
    shl eax, 21
    or eax, PAGE_PRESENT | PAGE_WRITE | 0x80
    mov [p2_table + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2

    mov eax, p4_table
    mov cr3, eax

    ret

enter_long_mode:
    mov esi, long_mode_msg
    call print_string_32

    mov eax, cr4
    or eax, CR4_PAE
    mov cr4, eax

    mov ecx, MSR_EFER
    rdmsr
    or eax, EFER_LME
    wrmsr

    mov eax, cr0
    or eax, CR0_PG | CR0_PE | CR0_MP
    mov cr0, eax

    ret

hang_32:
    cli
.loop:
    hlt
    jmp .loop

section .rodata
align 8
gdt64:
    dq 0
.code: equ $ - gdt64
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
.data: equ $ - gdt64
    dq (1 << 44) | (1 << 47) | (1 << 41)
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

[BITS 64]
long_mode_start:
    mov ax, gdt64.data
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, stack_top

    cld

    mov rdi, 0xB8000
    mov rcx, 80*25
    mov rax, 0x1F201F20
    rep stosq

    mov edi, [multiboot_info]
    call init_multiboot

    call init_global_constructors

    call kernel_main

    cli
.hang:
    hlt
    jmp .hang

debug_print:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    mov rdi, 0xB8000
    mov ah, 0x4F

.loop:
    lodsb
    test al, al
    jz .done

    mov [rdi], ax
    add rdi, 2
    jmp .loop

.done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

debug_print_hex:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rax

    mov rcx, 16
    lea rsi, [rel hex_chars]

.loop:
    rol rax, 4
    mov rbx, rax
    and rbx, 0xF
    mov bl, [rsi + rbx]
    mov bh, 0x4F
    mov [rdi], bx
    add rdi, 2
    dec rcx
    jnz .loop

    pop rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret