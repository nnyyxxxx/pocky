#include "printf.hpp"

#include "terminal.hpp"

extern "C" {

constexpr size_t PRINT_BUFFER_SIZE = 32;

void print_number(int num, int base) {
    if (num < 0) {
        terminal_putchar('-');
        num = -num;
    }

    char buffer[PRINT_BUFFER_SIZE] = {0};
    size_t i = 0;

    if (num == 0) {
        terminal_putchar('0');
        return;
    }

    while (num > 0 && i < PRINT_BUFFER_SIZE - 1) {
        int digit = num % base;
        buffer[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        num /= base;
    }

    while (i-- > 0)
        terminal_putchar(buffer[i]);
}

void print_unsigned(unsigned int num, int base) {
    char buffer[PRINT_BUFFER_SIZE] = {0};
    size_t i = 0;

    if (num == 0) {
        terminal_putchar('0');
        return;
    }

    while (num > 0 && i < PRINT_BUFFER_SIZE - 1) {
        unsigned int digit = num % base;
        buffer[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        num /= base;
    }

    while (i-- > 0)
        terminal_putchar(buffer[i]);
}

void print_hex(unsigned int num) {
    terminal_writestring("0x");
    if (num == 0) {
        terminal_putchar('0');
        return;
    }

    char buffer[PRINT_BUFFER_SIZE] = {0};
    size_t i = 0;

    while (num > 0 && i < PRINT_BUFFER_SIZE - 1) {
        int digit = num % 16;
        buffer[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        num /= 16;
    }

    while (i-- > 0)
        terminal_putchar(buffer[i]);
}

void print_pointer(const void* ptr) {
    print_hex(reinterpret_cast<unsigned long>(ptr));
}

int vprintf(const char* format, va_list args) {
    int written = 0;

    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd':
                    print_number(va_arg(args, int));
                    break;
                case 'u':
                    print_unsigned(va_arg(args, unsigned int));
                    break;
                case 'x':
                    print_hex(va_arg(args, unsigned int));
                    break;
                case 'p':
                    print_pointer(va_arg(args, void*));
                    break;
                case 's': {
                    const char* str = va_arg(args, const char*);
                    if (!str) str = "(null)";
                    terminal_writestring(str);
                    break;
                }
                case '%':
                    terminal_putchar('%');
                    break;
                default:
                    terminal_putchar('%');
                    terminal_putchar(*format);
                    break;
            }
        } else
            terminal_putchar(*format);
        format++;
        written++;
    }

    return written;
}

int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);
    return ret;
}

}  // extern "C"