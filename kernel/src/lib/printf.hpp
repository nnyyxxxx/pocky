#pragma once

#include <cstdarg>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

int printf(const char* format, ...);
int vprintf(const char* format, va_list args);

void print_number(int num, int base = 10);
void print_unsigned(unsigned int num, int base = 10);
void print_hex(unsigned int num);
void print_pointer(const void* ptr);

#ifdef __cplusplus
}
#endif