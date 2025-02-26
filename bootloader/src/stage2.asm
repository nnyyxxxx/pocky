[BITS 16]
[ORG 0x8000]

KERNEL_OFFSET equ 0x100000
KERNEL_SECTORS equ 24

start:
    mov si, msg_stage2
    call print_string

    mov si, msg_load_kernel
    call print_string
    call load_kernel

    mov si, msg_64bit
    call print_string

    cli
    call switch_to_pm


print_string:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print_string
.done:
    ret

load_kernel:
    mov ah, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, KERNEL_SECTORS
    mov ch, 0
    mov cl, 9
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    cmp al, KERNEL_SECTORS
    jne count_error
    ret

disk_error:
    mov si, msg_disk_error
    call print_string
    jmp $

count_error:
    mov si, msg_count_error
    call print_string
    jmp $


%include "bootloader/src/gdt.asm"
%include "bootloader/src/pm_switch.asm"

%include "bootloader/src/gdt64.asm"
%include "bootloader/src/long_mode.asm"
%include "bootloader/src/lm_switch.asm"

boot_drive db 0

msg_stage2 db 'Second-stage bootloader started', 13, 10, 0
msg_load_kernel db 'Loading kernel...', 13, 10, 0
msg_64bit db 'Transitioning to 64-bit mode...', 13, 10, 0
msg_disk_error db 'Disk error!', 13, 10, 0
msg_count_error db 'Sector count mismatch!', 13, 10, 0