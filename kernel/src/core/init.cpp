#include <cstdint>

extern "C" {
extern uintptr_t __init_array_start;
extern uintptr_t __init_array_end;
}

using ctor_func = void (*)();

extern "C" void init_global_constructors() {
    auto* start = reinterpret_cast<ctor_func*>(&__init_array_start);
    auto* end = reinterpret_cast<ctor_func*>(&__init_array_end);

    for (auto* ctor = start; ctor < end; ctor++) {
        (*ctor)();
    }
}