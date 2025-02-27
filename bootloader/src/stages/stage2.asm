[BITS 16]
[ORG 0x8000]

KERNEL_OFFSET equ 0x100000
KERNEL_SIZE_OFFSET equ 502
SCREEN_WIDTH equ 80
BOX_TOP_ROW equ 4
BOX_LEFT_COL equ 20
BOX_WIDTH equ 40
BOX_HEIGHT equ 14
ENTRY_WIDTH equ 30
ENTRY_PADDING equ 2
COLOR_WHITE_ON_BLACK equ 0x07
COLOR_BLACK_ON_WHITE equ 0x70
KEY_ENTER equ 0x1C

start:
    mov [boot_drive], dl
    call clear_screen
    call hide_cursor

    call display_header
    call draw_box
    call display_menu
    call display_navigation
    call handle_menu_input

    jmp $

clear_screen:
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    ret

hide_cursor:
    mov ah, 0x01
    mov ch, 0x3F
    int 0x10
    ret

display_header:
    mov si, msg_menu_header
    call get_string_length

    mov ax, SCREEN_WIDTH
    sub ax, cx
    shr ax, 1

    mov dh, BOX_TOP_ROW - 2
    mov dl, al
    call set_cursor

    mov si, msg_menu_header
    call print_string
    ret

display_navigation:
    mov si, msg_navigation_help
    call get_string_length

    mov ax, SCREEN_WIDTH
    sub ax, cx
    shr ax, 1

    mov dh, BOX_TOP_ROW + BOX_HEIGHT + 1
    mov dl, al
    call set_cursor

    mov si, msg_navigation_help
    call print_string
    ret

draw_box:
    mov dh, BOX_TOP_ROW
    mov dl, BOX_LEFT_COL
    call set_cursor
    mov al, 0xC9
    call print_char

    mov cx, BOX_WIDTH - 2
.top_border:
    mov al, 0xCD
    call print_char
    loop .top_border

    mov al, 0xBB
    call print_char

    mov cx, BOX_HEIGHT - 2
.sides_loop:
    mov dh, BOX_TOP_ROW
    add dh, cl
    mov dl, BOX_LEFT_COL
    call set_cursor
    mov al, 0xBA
    call print_char

    mov dh, BOX_TOP_ROW
    add dh, cl
    mov dl, BOX_LEFT_COL
    add dl, BOX_WIDTH - 1
    call set_cursor
    mov al, 0xBA
    call print_char
    loop .sides_loop

    mov dh, BOX_TOP_ROW + BOX_HEIGHT - 1
    mov dl, BOX_LEFT_COL
    call set_cursor
    mov al, 0xC8
    call print_char

    mov cx, BOX_WIDTH - 2
.bottom_border:
    mov al, 0xCD
    call print_char
    loop .bottom_border

    mov al, 0xBC
    call print_char

    ret

print_char:
    mov ah, 0x0E
    int 0x10
    ret

print_char_attr:
    mov ah, 0x09
    mov bh, 0
    mov cx, 1
    int 0x10
    ret

display_menu:
    mov dh, BOX_TOP_ROW + 2
    mov bl, COLOR_BLACK_ON_WHITE
    mov dl, BOX_LEFT_COL + ENTRY_PADDING
    call set_cursor

    mov cx, BOX_WIDTH - ENTRY_PADDING - 1
    mov al, ' '
.fill_bar:
    call print_char_attr
    inc dl
    call set_cursor
    loop .fill_bar

    mov dl, BOX_LEFT_COL + ENTRY_PADDING
    call set_cursor

    mov si, msg_kernel_entry
    call print_string_attr

    ret

print_string_attr:
    push si
    push dx
.loop:
    lodsb
    or al, al
    jz .done
    call print_char_attr
    inc dl
    call set_cursor
    jmp .loop
.done:
    pop dx
    pop si
    ret

get_string_length:
    push si
    xor cx, cx
.loop:
    lodsb
    or al, al
    jz .done
    inc cx
    jmp .loop
.done:
    pop si
    ret

handle_menu_input:
.input_loop:
    mov ah, 0
    int 0x16

    cmp ah, KEY_ENTER
    je .boot_kernel

    jmp .input_loop

.boot_kernel:
    call clear_screen
    call show_cursor

    mov si, msg_booting_kernel
    call print_string

    mov cx, 0xFFFF
.pause_loop:
    loop .pause_loop

    call load_kernel
    cli
    call switch_to_pm

show_cursor:
    mov ah, 0x01
    mov cx, 0x0607
    int 0x10
    ret

set_cursor:
    mov ah, 0x02
    mov bh, 0
    int 0x10
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

    mov ax, 0x0000
    mov es, ax
    mov bx, 0x7C00
    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, 1
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov al, [es:bx + KERNEL_SIZE_OFFSET]
    mov [kernel_sectors], al

    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, [kernel_sectors]
    mov ch, 0
    mov cl, 9
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    cmp al, [kernel_sectors]
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
kernel_sectors db 0

msg_menu_header db 'Boot Menu', 0
msg_kernel_entry db 'Kernel', 0
msg_navigation_help db 'Select: Enter', 0
msg_booting_kernel db 'Booting kernel...', 13, 10, 0
msg_disk_error db 'Disk error!', 13, 10, 0
msg_count_error db 'Sector count mismatch!', 13, 10, 0