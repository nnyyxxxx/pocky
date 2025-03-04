#include "printf.hpp"

#include "terminal.hpp"

extern "C" {

constexpr size_t PRINT_BUFFER_SIZE = 32;

void print_number(int num, int base, int width, bool pad_zero) {
    bool negative = num < 0;
    if (negative) num = -num;

    char buffer[PRINT_BUFFER_SIZE] = {0};
    size_t i = 0;

    if (num == 0)
        buffer[i++] = '0';
    else {
        while (num > 0 && i < PRINT_BUFFER_SIZE - 1) {
            int digit = num % base;
            buffer[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
            num /= base;
        }
    }

    int padding = width - i - (negative ? 1 : 0);
    if (pad_zero && negative) terminal_putchar('-');

    while (padding-- > 0)
        terminal_putchar(pad_zero ? '0' : ' ');

    if (!pad_zero && negative) terminal_putchar('-');

    while (i-- > 0)
        terminal_putchar(buffer[i]);
}

void print_unsigned(unsigned long num, int base, int width, bool pad_zero) {
    char buffer[PRINT_BUFFER_SIZE] = {0};
    size_t i = 0;

    if (num == 0)
        buffer[i++] = '0';
    else {
        while (num > 0 && i < PRINT_BUFFER_SIZE - 1) {
            unsigned long digit = num % base;
            buffer[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
            num /= base;
        }
    }

    int padding = width - i;
    while (padding-- > 0)
        terminal_putchar(pad_zero ? '0' : ' ');

    while (i-- > 0)
        terminal_putchar(buffer[i]);
}

void print_hex(unsigned long num, int width, bool pad_zero) {
    char buffer[PRINT_BUFFER_SIZE] = {0};
    size_t i = 0;

    if (num == 0)
        buffer[i++] = '0';
    else {
        while (num > 0 && i < PRINT_BUFFER_SIZE - 1) {
            int digit = num % 16;
            buffer[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
            num /= 16;
        }
    }

    printf("0x");
    int padding = width - i;
    while (padding-- > 0)
        terminal_putchar(pad_zero ? '0' : ' ');

    while (i-- > 0)
        terminal_putchar(buffer[i]);
}

void print_pointer(const void* ptr) {
    print_hex(reinterpret_cast<unsigned long>(ptr));
}

void print_float(double num, int precision) {
    if (num < 0) {
        terminal_putchar('-');
        num = -num;
    }

    unsigned long integer_part = static_cast<unsigned long>(num);
    print_unsigned(integer_part, 10, 0, false);

    terminal_putchar('.');

    double fractional = num - integer_part;
    for (int i = 0; i < precision; i++) {
        fractional *= 10;
        int digit = static_cast<int>(fractional);
        terminal_putchar('0' + digit);
        fractional -= digit;
    }
}

int vprintf(const char* format, va_list args) {
    int written = 0;

    while (*format) {
        if (*format == '%') {
            format++;

            int width = 0;
            bool pad_zero = false;
            int precision = -1;

            if (*format == '0') {
                pad_zero = true;
                format++;
            }

            while (*format >= '0' && *format <= '9') {
                width = width * 10 + (*format - '0');
                format++;
            }

            if (*format == '.') {
                format++;
                precision = 0;
                while (*format >= '0' && *format <= '9') {
                    precision = precision * 10 + (*format - '0');
                    format++;
                }
            }

            bool is_size_t = false;
            if (*format == 'z') {
                is_size_t = true;
                format++;
            }

            bool is_long = false;
            if (*format == 'l') {
                is_long = true;
                format++;
            }

            switch (*format) {
                case 'f': {
                    if (precision < 0) precision = 6;
                    print_float(va_arg(args, double), precision);
                    break;
                }
                case 'd':
                    if (is_size_t)
                        print_unsigned(va_arg(args, size_t), 10, width, pad_zero);
                    else if (is_long)
                        print_number(va_arg(args, long), 10, width, pad_zero);
                    else
                        print_number(va_arg(args, int), 10, width, pad_zero);
                    break;
                case 'u':
                    if (is_size_t || is_long)
                        print_unsigned(va_arg(args, unsigned long), 10, width, pad_zero);
                    else
                        print_unsigned(va_arg(args, unsigned int), 10, width, pad_zero);
                    break;
                case 'x':
                    if (is_size_t || is_long)
                        print_hex(va_arg(args, unsigned long), width, pad_zero);
                    else
                        print_hex(va_arg(args, unsigned int), width, pad_zero);
                    break;
                case 'p':
                    print_pointer(va_arg(args, void*));
                    break;
                case 's': {
                    const char* str = va_arg(args, const char*);
                    if (!str) str = "(null)";
                    printf(str);
                    break;
                }
                case '%':
                    terminal_putchar('%');
                    break;
                case 'c':
                    terminal_putchar(va_arg(args, int));
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