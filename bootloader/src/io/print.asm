print_hex:
    push ax
    push cx

    mov cl, 4
    shr al, cl
    call .print_digit

    pop cx
    pop ax
    and al, 0x0F
    call .print_digit
    ret

.print_digit:
    cmp al, 10
    jl .is_digit
    add al, 'A' - 10 - '0'
.is_digit:
    add al, '0'
    mov ah, 0x0E
    int 0x10
    ret

print_string:
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    ret