[BITS 32]
switch_to_long_mode:
    call check_long_mode

    call setup_paging

    call enable_long_mode

    lgdt [gdt64_descriptor]

    jmp CODE_SEG:long_mode_entry

[BITS 64]
long_mode_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rax, 0x1F201F201F201F20
    mov rdi, 0xB8000
    mov rcx, 500
    rep stosq

    mov rsi, 0x10000
    mov rdi, KERNEL_OFFSET
    movzx rcx, byte [kernel_sectors]
    shl rcx, 9
    shr rcx, 3
    rep movsq

    mov dword [0xB8000], 0x1F4B1F4F

    mov rax, KERNEL_OFFSET
    jmp rax