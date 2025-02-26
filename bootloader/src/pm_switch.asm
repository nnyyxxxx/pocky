switch_to_pm:
    in al, 0x92
    or al, 2
    out 0x92, al

    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE_SEG:protected_mode_entry

[BITS 32]
protected_mode_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x90000

    mov esi, 0x10000
    mov edi, KERNEL_OFFSET
    mov ecx, KERNEL_SECTORS * 512 / 4
    rep movsd

    mov dword [0xB8000], 0x2F4B2F4F

    mov ecx, 10000000
.delay_loop:
    loop .delay_loop

    jmp KERNEL_OFFSET