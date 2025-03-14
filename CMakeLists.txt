cmake_minimum_required(VERSION 3.12)
project(pocky LANGUAGES CXX ASM)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_ASM_NASM_COMPILER nasm)

set(CMAKE_CXX_FLAGS "-m64 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fstack-protector-strong")

set(KERNEL_SRC ${CMAKE_SOURCE_DIR}/kernel/src)
set(BUILD_DIR ${CMAKE_BINARY_DIR})

include_directories(
    ${KERNEL_SRC}
    ${KERNEL_SRC}/hw
    ${KERNEL_SRC}/core
    ${KERNEL_SRC}/shell
    ${KERNEL_SRC}/drivers
    ${KERNEL_SRC}/lib
    ${KERNEL_SRC}/memory
    ${KERNEL_SRC}/fs
)

set(KERNEL_CPP_SRCS
    ${KERNEL_SRC}/core/kernel.cpp
    ${KERNEL_SRC}/lib/lib.cpp
    ${KERNEL_SRC}/hw/io.cpp
    ${KERNEL_SRC}/shell/vga.cpp
    ${KERNEL_SRC}/shell/terminal.cpp
    ${KERNEL_SRC}/drivers/keyboard.cpp
    ${KERNEL_SRC}/drivers/ata.cpp
    ${KERNEL_SRC}/hw/pic.cpp
    ${KERNEL_SRC}/shell/shell.cpp
    ${KERNEL_SRC}/hw/gdt.cpp
    ${KERNEL_SRC}/hw/idt.cpp
    ${KERNEL_SRC}/hw/acpi.cpp
    ${KERNEL_SRC}/hw/smp.cpp
    ${KERNEL_SRC}/memory/physical_memory.cpp
    ${KERNEL_SRC}/memory/virtual_memory.cpp
    ${KERNEL_SRC}/memory/heap.cpp
    ${KERNEL_SRC}/core/init.cpp
    ${KERNEL_SRC}/core/elf.cpp
    ${KERNEL_SRC}/core/dynamic_linker.cpp
    ${KERNEL_SRC}/lib/printf.cpp
    ${KERNEL_SRC}/fs/fat32.cpp
    ${KERNEL_SRC}/hw/timer.cpp
    ${KERNEL_SRC}/shell/editor.cpp
    ${KERNEL_SRC}/core/multiboot2.cpp
    ${KERNEL_SRC}/core/process.cpp
    ${KERNEL_SRC}/hw/rtc.cpp
    ${KERNEL_SRC}/shell/pager.cpp
    ${KERNEL_SRC}/shell/screen_state.cpp
    ${KERNEL_SRC}/core/scheduler.cpp
    ${KERNEL_SRC}/core/ipc.cpp
    ${KERNEL_SRC}/lib/cxxabi.cpp
    ${KERNEL_SRC}/shell/commands/help.cpp
    ${KERNEL_SRC}/shell/commands/echo.cpp
    ${KERNEL_SRC}/shell/commands/crash.cpp
    ${KERNEL_SRC}/shell/commands/shutdown.cpp
    ${KERNEL_SRC}/shell/commands/memory.cpp
    ${KERNEL_SRC}/shell/commands/ls.cpp
    ${KERNEL_SRC}/shell/commands/mkdir.cpp
    ${KERNEL_SRC}/shell/commands/cd.cpp
    ${KERNEL_SRC}/shell/commands/cat.cpp
    ${KERNEL_SRC}/shell/commands/mv.cpp
    ${KERNEL_SRC}/shell/commands/rm.cpp
    ${KERNEL_SRC}/shell/commands/touch.cpp
    ${KERNEL_SRC}/shell/commands/edit.cpp
    ${KERNEL_SRC}/shell/commands/history.cpp
    ${KERNEL_SRC}/shell/commands/uptime.cpp
    ${KERNEL_SRC}/shell/commands/time.cpp
    ${KERNEL_SRC}/shell/commands/ps.cpp
    ${KERNEL_SRC}/shell/commands/pkill.cpp
    ${KERNEL_SRC}/shell/commands/less.cpp
    ${KERNEL_SRC}/shell/commands/ipc_test.cpp
    ${KERNEL_SRC}/shell/commands/cores.cpp
    ${KERNEL_SRC}/shell/commands/alias.cpp
)

set(KERNEL_ASM_SRCS
    ${KERNEL_SRC}/asm/entry.asm
    ${KERNEL_SRC}/asm/gdt.asm
    ${KERNEL_SRC}/asm/idt.asm
    ${KERNEL_SRC}/asm/isr.asm
    ${KERNEL_SRC}/asm/vm.asm
    ${KERNEL_SRC}/asm/timer.asm
    ${KERNEL_SRC}/asm/context.asm
    ${KERNEL_SRC}/asm/ap_boot.asm
)

set(ASM_OBJECTS "")
foreach(ASM_FILE ${KERNEL_ASM_SRCS})
    get_filename_component(ASM_NAME ${ASM_FILE} NAME_WE)
    set(ASM_OUT "${CMAKE_CURRENT_BINARY_DIR}/${ASM_NAME}_asm.o")
    add_custom_command(
        OUTPUT ${ASM_OUT}
        COMMAND ${CMAKE_ASM_NASM_COMPILER} -f elf64 ${ASM_FILE} -o ${ASM_OUT}
        DEPENDS ${ASM_FILE}
        COMMENT "Assembling ${ASM_FILE}"
    )
    list(APPEND ASM_OBJECTS ${ASM_OUT})
endforeach()

add_custom_target(asm_objects DEPENDS ${ASM_OBJECTS})

add_library(kernel_lib STATIC ${KERNEL_CPP_SRCS})
add_dependencies(kernel_lib asm_objects)

add_custom_target(kernel ALL
    COMMAND ${CMAKE_LINKER} -T ${CMAKE_SOURCE_DIR}/kernel/linker.ld -m elf_x86_64 -nostdlib
            ${ASM_OBJECTS} $<TARGET_FILE:kernel_lib> -o ${CMAKE_BINARY_DIR}/kernel_elf.bin
    COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_BINARY_DIR}/kernel_elf.bin
            ${CMAKE_BINARY_DIR}/kernel.bin
    DEPENDS kernel_lib ${ASM_OBJECTS}
    COMMENT "Linking kernel and creating binary"
)