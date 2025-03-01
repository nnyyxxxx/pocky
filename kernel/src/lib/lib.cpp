#include <cstddef>
#include <cstdint>

#include "memory/heap.hpp"
#include "terminal.hpp"

extern "C" size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

extern "C" int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

extern "C" int strncmp(const char* s1, const char* s2, size_t n) {
    while (n-- > 0) {
        if (*s1 != *s2) return *(const unsigned char*)s1 - *(const unsigned char*)s2;

        if (*s1 == '\0') return 0;

        s1++;
        s2++;
    }

    return 0;
}

extern "C" void* memset(void* dest, int ch, size_t count) {
    uint8_t* d = reinterpret_cast<uint8_t*>(dest);
    for (size_t i = 0; i < count; i++)
        d[i] = static_cast<uint8_t>(ch);
    return dest;
}

extern "C" void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = reinterpret_cast<uint8_t*>(dest);
    const uint8_t* s = reinterpret_cast<const uint8_t*>(src);
    for (size_t i = 0; i < count; i++)
        d[i] = s[i];
    return dest;
}

extern "C" void* memmove(void* dest, const void* src, size_t count) {
    unsigned char* d = static_cast<unsigned char*>(dest);
    const unsigned char* s = static_cast<const unsigned char*>(src);

    if (d < s) {
        while (count--) {
            *d++ = *s++;
        }
    } else {
        d += count;
        s += count;
        while (count--) {
            *--d = *--s;
        }
    }

    return dest;
}

extern "C" [[noreturn]] void __stack_chk_fail() {
    terminal_writestring("\nStack smashing detected! Aborting...\n");
    while (true) {
        asm volatile("cli; hlt");
    }
}

uintptr_t __stack_chk_guard = 0xDEADBEEFDEADBEEF;

extern "C" int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = static_cast<const unsigned char*>(s1);
    const unsigned char* p2 = static_cast<const unsigned char*>(s2);

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return p1[i] - p2[i];
    }

    return 0;
}

extern "C" char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++))
        ;
    return dest;
}

extern "C" char* strncpy(char* dest, const char* src, size_t count) {
    char* d = dest;
    while (count > 0 && *src) {
        *d++ = *src++;
        count--;
    }
    while (count > 0) {
        *d++ = '\0';
        count--;
    }
    return dest;
}

extern "C" char* strchr(const char* str, int ch) {
    while (*str && *str != ch)
        str++;
    return *str == ch ? const_cast<char*>(str) : nullptr;
}

extern "C" char* strrchr(const char* str, int ch) {
    const char* last = nullptr;
    while (*str) {
        if (*str == ch) last = str;
        str++;
    }
    return const_cast<char*>(last);
}

extern "C" int snprintf(char* str, size_t size, const char* format, ...) {
    if (size == 0) return 0;
    strncpy(str, format, size - 1);
    str[size - 1] = '\0';
    return strlen(str);
}

extern "C" char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) {
        d++;
    }
    while ((*d++ = *src++)) {
        ;
    }
    return dest;
}

void* operator new(size_t size) {
    return HeapAllocator::instance().allocate(size);
}

void* operator new[](size_t size) {
    return HeapAllocator::instance().allocate(size);
}

void operator delete(void* ptr) noexcept {
    HeapAllocator::instance().free(ptr);
}

void operator delete[](void* ptr) noexcept {
    HeapAllocator::instance().free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    HeapAllocator::instance().free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    HeapAllocator::instance().free(ptr);
}