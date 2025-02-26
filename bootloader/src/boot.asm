[BITS 16]
[ORG 0x7C00]

KERNEL_OFFSET equ 0x100000
KERNEL_SECTORS equ 24

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl

    mov ax, 0x0003
    int 0x10

    mov si, msg_boot
    call print_string

    mov si, msg_load
    call print_string
    call load_kernel

    mov si, msg_success
    call print_string

    cli

    call switch_to_pm

%include "bootloader/src/disk.asm"
%include "bootloader/src/print.asm"
%include "bootloader/src/gdt.asm"
%include "bootloader/src/pm_switch.asm"

boot_drive db 0
disk_error_code db 0

msg_boot db 'Bootloader starting...', 13, 10, 0
msg_load db 'Loading kernel...', 13, 10, 0
msg_success db 'Kernel loaded successfully!', 13, 10, 0

times 510-($-$$) db 0
dw 0xAA55