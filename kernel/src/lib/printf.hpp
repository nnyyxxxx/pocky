#pragma once

#include <cstdarg>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

int printf(const char* format, ...);
int vprintf(const char* format, va_list args);

void print_number(int num, int base = 10, int width = 0, bool pad_zero = false,
                  bool left_justify = false);
void print_unsigned(unsigned long long num, int base = 10, int width = 0, bool pad_zero = false,
                    bool left_justify = false);
void print_hex(unsigned long long num, int width = 0, bool pad_zero = false,
               bool left_justify = false);
void print_pointer(const void* ptr);

#ifdef __cplusplus
}
#endif