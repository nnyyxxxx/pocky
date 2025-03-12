[BITS 16]
section .ap_trampoline
global g_ap_trampoline_start
global g_ap_trampoline_end
global g_ap_ready_count
global g_ap_lock
global g_ap_target_cpu

g_ap_trampoline_start:
    cli

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    lgdt [cs:ap_gdtr - g_ap_trampoline_start]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp dword 0x08:(ap_protected_mode - g_ap_trampoline_start + 0x8000)

[BITS 32]
ap_protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x9000

    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    mov eax, [ap_cr3 - g_ap_trampoline_start + 0x8000]
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax

    jmp 0x18:(ap_long_mode - g_ap_trampoline_start + 0x8000)

[BITS 64]
ap_long_mode:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov edi, [g_ap_target_cpu]

    call ap_main

ap_halt:
    cli
    hlt
    jmp ap_halt

ap_gdt:
    dq 0

    dw 0xFFFF
    dw 0
    db 0
    db 0x9A
    db 0xCF
    db 0

    dw 0xFFFF
    dw 0
    db 0
    db 0x92
    db 0xCF
    db 0

    dw 0
    dw 0
    db 0
    db 0x9A
    db 0xAF
    db 0

    dw 0
    dw 0
    db 0
    db 0x92
    db 0xAF
    db 0

ap_gdtr:
    dw (5 * 8) - 1
    dq ap_gdt

ap_cr3:
    dq 0

g_ap_ready_count:
    dd 0
g_ap_lock:
    dd 0
g_ap_target_cpu:
    dd 0

align 16
g_ap_trampoline_end:

extern ap_main