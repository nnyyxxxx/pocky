[BITS 16]
[ORG 0x8000]

KERNEL_OFFSET equ 0x100000
KERNEL_SECTORS equ 24

start:
    call clear_screen

    mov si, msg_boot_prompt
    call print_string
    call wait_key

    call load_kernel
    cli
    call switch_to_pm

clear_screen:
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    ret

wait_key:
    mov ah, 0
    int 0x16
    ret

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


%include "memory/gdt.asm"
%include "mode/pm_switch.asm"

%include "memory/gdt64.asm"
%include "mode/long_mode.asm"
%include "mode/lm_switch.asm"

boot_drive db 0

msg_boot_prompt db 'Press any key to boot into kernel...', 13, 10, 0
msg_disk_error db 'Disk error!', 13, 10, 0
msg_count_error db 'Sector count mismatch!', 13, 10, 0