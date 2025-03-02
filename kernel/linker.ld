ENTRY(_start)

SECTIONS {
    . = 1M;

    .multiboot_header ALIGN(8) : {
        *(.multiboot_header)
    }

    .text ALIGN(4K) : {
        *(.text.boot)
        *(.text .text.*)
    }

    .rodata ALIGN(4K) : {
        *(.rodata .rodata.*)
    }

    .init_array ALIGN(4K) : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);
    }

    .fini_array : {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN(__fini_array_end = .);
    }

    .data ALIGN(4K) : {
        *(.data .data.*)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss .bss.*)
    }
}