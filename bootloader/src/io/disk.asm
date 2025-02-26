load_kernel:
    mov ah, 0
    mov dl, [boot_drive]
    int 0x13
    jc .disk_error

    mov bx, 0x1000
    mov es, bx
    xor bx, bx

    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc .sector_error

    mov ah, 0x02
    mov al, KERNEL_SECTORS
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc .sector_error

    cmp al, KERNEL_SECTORS
    jne .count_error

    ret

.disk_error:
    mov si, msg_disk_reset_error
    call print_string
    mov ah, 0x01
    int 0x13
    mov al, ah
    call print_hex
    jmp $

.sector_error:
    mov si, msg_disk_read_error
    call print_string
    mov ah, 0x01
    int 0x13
    mov al, ah
    call print_hex
    jmp $

.count_error:
    mov si, msg_count_error
    call print_string
    jmp $

msg_disk_reset_error db 'Disk reset error! Code: 0x', 0
msg_disk_read_error db 'Disk read error! Code: 0x', 0
msg_count_error db 'Sector count mismatch!', 13, 10, 0