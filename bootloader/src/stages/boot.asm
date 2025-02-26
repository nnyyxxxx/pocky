[BITS 16]
[ORG 0x7C00]

SECOND_STAGE_SECTORS equ 8
SECOND_STAGE_ADDRESS equ 0x8000

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

    mov si, msg_load_stage2
    call print_string
    call load_second_stage

    mov si, msg_jump_stage2
    call print_string
    jmp SECOND_STAGE_ADDRESS

print_string:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print_string
.done:
    ret

load_second_stage:
    mov ah, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov bx, SECOND_STAGE_ADDRESS
    mov ah, 0x02
    mov al, SECOND_STAGE_SECTORS
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    cmp al, SECOND_STAGE_SECTORS
    jne disk_error
    ret

disk_error:
    mov si, msg_disk_error
    call print_string
    jmp $

boot_drive db 0

msg_boot db 'First-stage boot', 13, 10, 0
msg_load_stage2 db 'Loading second stage...', 13, 10, 0
msg_jump_stage2 db 'Jumping to second stage...', 13, 10, 0
msg_disk_error db 'Disk error!', 13, 10, 0

times 510-($-$$) db 0
dw 0xAA55