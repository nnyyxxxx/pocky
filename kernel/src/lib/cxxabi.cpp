#include <cstddef>
#include <cstdint>

extern "C" {

int __cxa_guard_acquire(uint64_t* guard_object) {
    return ((*guard_object) & 1) == 0;
}

void __cxa_guard_release(uint64_t* guard_object) {
    *guard_object = 1;
}

void __cxa_guard_abort(uint64_t* guard_object) {
    *guard_object = 0;
}

int __cxa_atexit() {
    return 0;
}

void __cxa_pure_virtual() {
    while (1) {
        asm volatile("hlt");
    }
}

void* __dso_handle = nullptr;

}  // extern "C"