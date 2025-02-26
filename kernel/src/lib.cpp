#include <cstddef>
#include <cstdint>

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
    unsigned char* ptr = static_cast<unsigned char*>(dest);
    while (count-- > 0) {
        *ptr++ = ch;
    }
    return dest;
}

extern "C" void* memcpy(void* dest, const void* src, size_t count) {
    unsigned char* d = static_cast<unsigned char*>(dest);
    const unsigned char* s = static_cast<const unsigned char*>(src);
    while (count--) {
        *d++ = *s++;
    }
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
    // TODO: add panic handling

    while (true) {
        asm volatile("cli; hlt");
    }
}

extern "C" uintptr_t __stack_chk_guard;
uintptr_t __stack_chk_guard = 0xDEADBEEFDEADBEEF;