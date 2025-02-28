#include "idt.hpp"

#include <cstring>

#include "io.hpp"
#include "keyboard.hpp"
#include "pic.hpp"
#include "shell.hpp"
#include "terminal.hpp"

namespace {

constexpr size_t IDT_ENTRIES = 256;
IDTEntry idt[IDT_ENTRIES] = {};
IDTDescriptor idtr = {sizeof(idt) - 1, 0};

const char* exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point",
    "Virtualization",
    "Control Protection",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
};

void create_descriptor(IDTEntry& entry, void (*handler)(), uint16_t selector, uint8_t flags) {
    uint64_t addr = reinterpret_cast<uint64_t>(handler);
    entry.offset_low = addr & 0xFFFF;
    entry.offset_middle = (addr >> 16) & 0xFFFF;
    entry.offset_high = (addr >> 32) & 0xFFFFFFFF;
    entry.selector = selector;
    entry.ist = 0;
    entry.type_attr = flags;
    entry.zero = 0;
}

extern "C" void load_idt(IDTDescriptor* idtr);

extern "C" void isr0();
extern "C" void isr1();
extern "C" void isr2();
extern "C" void isr3();
extern "C" void isr4();
extern "C" void isr5();
extern "C" void isr6();
extern "C" void isr7();
extern "C" void isr8();
extern "C" void isr9();
extern "C" void isr10();
extern "C" void isr11();
extern "C" void isr12();
extern "C" void isr13();
extern "C" void isr14();
extern "C" void isr15();
extern "C" void isr16();
extern "C" void isr17();
extern "C" void isr18();
extern "C" void isr19();
extern "C" void isr20();
extern "C" void isr21();
extern "C" void isr22();
extern "C" void isr23();
extern "C" void isr24();
extern "C" void isr25();
extern "C" void isr26();
extern "C" void isr27();
extern "C" void isr28();
extern "C" void isr29();
extern "C" void isr30();
extern "C" void isr31();

extern "C" void isr32();
extern "C" void isr33();
extern "C" void isr34();
extern "C" void isr35();
extern "C" void isr36();
extern "C" void isr37();
extern "C" void isr38();
extern "C" void isr39();
extern "C" void isr40();
extern "C" void isr41();
extern "C" void isr42();
extern "C" void isr43();
extern "C" void isr44();
extern "C" void isr45();
extern "C" void isr46();
extern "C" void isr47();

}  // namespace

struct InterruptFrame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t interrupt_number, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

extern "C" void isr_handler(InterruptFrame* frame) {
    if (frame->interrupt_number < 32) {
        handling_exception = true;

        if (frame->interrupt_number == 6) {
            static int invalid_op_count = 0;
            invalid_op_count++;

            if (invalid_op_count > 1) {
                terminal_writestring("\nRecursive invalid opcode detected! Halting...\n");
                asm volatile("cli; hlt");
                return;
            }

            terminal_writestring("\nInvalid Opcode Details:\n");
            terminal_writestring("RIP: 0x");
            char rip[17];
            for (int i = 15; i >= 0; i--) {
                int digit = (frame->rip >> (i * 4)) & 0xF;
                rip[15 - i] = digit < 10 ? '0' + digit : 'A' + digit - 10;
            }
            rip[16] = '\0';
            terminal_writestring(rip);

            terminal_writestring("\nInstruction bytes: ");
            unsigned char* code = (unsigned char*)frame->rip;
            for (int i = 0; i < 8; i++) {
                char hex[3];
                unsigned char byte = code[i];
                hex[0] = "0123456789ABCDEF"[byte >> 4];
                hex[1] = "0123456789ABCDEF"[byte & 0xF];
                hex[2] = '\0';
                terminal_writestring(hex);
                terminal_writestring(" ");
            }

            terminal_writestring("\n\nRegister State:");
            terminal_writestring("\nRAX: 0x");
            char reg[17];
            for (int i = 15; i >= 0; i--) {
                int digit = (frame->rax >> (i * 4)) & 0xF;
                reg[15 - i] = digit < 10 ? '0' + digit : 'A' + digit - 10;
            }
            reg[16] = '\0';
            terminal_writestring(reg);

            terminal_writestring("\nRSP: 0x");
            for (int i = 15; i >= 0; i--) {
                int digit = (frame->rsp >> (i * 4)) & 0xF;
                reg[15 - i] = digit < 10 ? '0' + digit : 'A' + digit - 10;
            }
            terminal_writestring(reg);

            terminal_writestring("\nCS: 0x");
            for (int i = 3; i >= 0; i--) {
                int digit = (frame->cs >> (i * 4)) & 0xF;
                reg[3 - i] = digit < 10 ? '0' + digit : 'A' + digit - 10;
            }
            reg[4] = '\0';
            terminal_writestring(reg);

            terminal_writestring("\nRFLAGS: 0x");
            for (int i = 15; i >= 0; i--) {
                int digit = (frame->rflags >> (i * 4)) & 0xF;
                reg[15 - i] = digit < 10 ? '0' + digit : 'A' + digit - 10;
            }
            reg[16] = '\0';
            terminal_writestring(reg);

            terminal_writestring("\n\nSystem halted.\n");
            asm volatile("cli; hlt");
            return;
        }

        if (frame->interrupt_number == 14) {
            static int page_fault_count = 0;
            page_fault_count++;

            if (page_fault_count > 1) {
                terminal_writestring("\nRecursive page fault detected! Halting...\n");
                asm volatile("cli; hlt");
                return;
            }

            uint64_t fault_address;
            asm volatile("mov %%cr2, %0" : "=r"(fault_address));

            terminal_writestring("\nPage Fault Details:\n");
            terminal_writestring("Fault Address: 0x");

            char addr_hex[17];
            for (int i = 15; i >= 0; i--) {
                int digit = (fault_address >> (i * 4)) & 0xF;
                addr_hex[15 - i] = digit < 10 ? '0' + digit : 'A' + digit - 10;
            }
            addr_hex[16] = '\0';
            terminal_writestring(addr_hex);

            terminal_writestring("\nPage Table Indices:\n");
            terminal_writestring("  PML4 Index: ");
            char idx[5];
            uint64_t pml4_idx = (fault_address >> 39) & 0x1FF;
            for (int i = 0; i < 4; i++) {
                idx[3 - i] = ((pml4_idx >> (i * 4)) & 0xF) < 10
                                 ? '0' + ((pml4_idx >> (i * 4)) & 0xF)
                                 : 'A' + (((pml4_idx >> (i * 4)) & 0xF) - 10);
            }
            idx[4] = '\0';
            terminal_writestring(idx);

            terminal_writestring("\n  PDPT Index: ");
            uint64_t pdpt_idx = (fault_address >> 30) & 0x1FF;
            for (int i = 0; i < 4; i++) {
                idx[3 - i] = ((pdpt_idx >> (i * 4)) & 0xF) < 10
                                 ? '0' + ((pdpt_idx >> (i * 4)) & 0xF)
                                 : 'A' + (((pdpt_idx >> (i * 4)) & 0xF) - 10);
            }
            terminal_writestring(idx);

            terminal_writestring("\n  PD Index: ");
            uint64_t pd_idx = (fault_address >> 21) & 0x1FF;
            for (int i = 0; i < 4; i++) {
                idx[3 - i] = ((pd_idx >> (i * 4)) & 0xF) < 10
                                 ? '0' + ((pd_idx >> (i * 4)) & 0xF)
                                 : 'A' + (((pd_idx >> (i * 4)) & 0xF) - 10);
            }
            terminal_writestring(idx);

            terminal_writestring("\n  PT Index: ");
            uint64_t pt_idx = (fault_address >> 12) & 0x1FF;
            for (int i = 0; i < 4; i++) {
                idx[3 - i] = ((pt_idx >> (i * 4)) & 0xF) < 10
                                 ? '0' + ((pt_idx >> (i * 4)) & 0xF)
                                 : 'A' + (((pt_idx >> (i * 4)) & 0xF) - 10);
            }
            terminal_writestring(idx);

            terminal_writestring("\nError Code Bits:\n");
            terminal_writestring("  P (Present): ");
            terminal_writestring((frame->error_code & 0x1) ? "1\n" : "0\n");
            terminal_writestring("  W (Write): ");
            terminal_writestring((frame->error_code & 0x2) ? "1\n" : "0\n");
            terminal_writestring("  U (User): ");
            terminal_writestring((frame->error_code & 0x4) ? "1\n" : "0\n");
            terminal_writestring("  RSVD: ");
            terminal_writestring((frame->error_code & 0x8) ? "1\n" : "0\n");
            terminal_writestring("  I/D (Instruction Fetch): ");
            terminal_writestring((frame->error_code & 0x10) ? "1\n" : "0\n");

            terminal_writestring("\nSystem halted.\n");
            asm volatile("cli; hlt");
            return;
        }

        terminal_writestring("\nCPU Exception: ");
        terminal_writestring(exception_messages[frame->interrupt_number]);
        terminal_writestring("\nError Code: 0x");

        char error_code[32];
        for (int i = 0; i < 16; i++) {
            uint8_t nibble = (frame->error_code >> (60 - i * 4)) & 0xF;
            error_code[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        }
        error_code[16] = '\0';
        terminal_writestring(error_code);

        terminal_writestring("\nInstruction Pointer: 0x");
        char rip[32];
        for (int i = 0; i < 16; i++) {
            uint8_t nibble = (frame->rip >> (60 - i * 4)) & 0xF;
            rip[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        }
        rip[16] = '\0';
        terminal_writestring(rip);
        terminal_writestring("\n\n");

        memset(input_buffer, 0, sizeof(input_buffer));
        input_pos = 0;

        process_command();
        return;
    } else if (frame->interrupt_number < 48) {
        uint8_t irq = frame->interrupt_number - 32;

        if (irq >= 8) outb(PIC2_COMMAND, 0x20);
        outb(PIC1_COMMAND, 0x20);

        if (irq == 1) {
            char c = keyboard_read();
            if (c != 0) process_keypress(c);
        }
    }
}

void init_idt() {
    for (uint8_t vector = 0; vector < 32; vector++) {
        void (*handler)() = nullptr;
        switch (vector) {
            case 0:
                handler = isr0;
                break;
            case 1:
                handler = isr1;
                break;
            case 2:
                handler = isr2;
                break;
            case 3:
                handler = isr3;
                break;
            case 4:
                handler = isr4;
                break;
            case 5:
                handler = isr5;
                break;
            case 6:
                handler = isr6;
                break;
            case 7:
                handler = isr7;
                break;
            case 8:
                handler = isr8;
                break;
            case 9:
                handler = isr9;
                break;
            case 10:
                handler = isr10;
                break;
            case 11:
                handler = isr11;
                break;
            case 12:
                handler = isr12;
                break;
            case 13:
                handler = isr13;
                break;
            case 14:
                handler = isr14;
                break;
            case 15:
                handler = isr15;
                break;
            case 16:
                handler = isr16;
                break;
            case 17:
                handler = isr17;
                break;
            case 18:
                handler = isr18;
                break;
            case 19:
                handler = isr19;
                break;
            case 20:
                handler = isr20;
                break;
            case 21:
                handler = isr21;
                break;
            case 22:
                handler = isr22;
                break;
            case 23:
                handler = isr23;
                break;
            case 24:
                handler = isr24;
                break;
            case 25:
                handler = isr25;
                break;
            case 26:
                handler = isr26;
                break;
            case 27:
                handler = isr27;
                break;
            case 28:
                handler = isr28;
                break;
            case 29:
                handler = isr29;
                break;
            case 30:
                handler = isr30;
                break;
            case 31:
                handler = isr31;
                break;
        }
        set_interrupt_handler(vector, handler, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT_GATE);
    }

    for (uint8_t irq = 0; irq < 16; irq++) {
        void (*handler)() = nullptr;
        switch (irq) {
            case 0:
                handler = isr32;
                break;
            case 1:
                handler = isr33;
                break;
            case 2:
                handler = isr34;
                break;
            case 3:
                handler = isr35;
                break;
            case 4:
                handler = isr36;
                break;
            case 5:
                handler = isr37;
                break;
            case 6:
                handler = isr38;
                break;
            case 7:
                handler = isr39;
                break;
            case 8:
                handler = isr40;
                break;
            case 9:
                handler = isr41;
                break;
            case 10:
                handler = isr42;
                break;
            case 11:
                handler = isr43;
                break;
            case 12:
                handler = isr44;
                break;
            case 13:
                handler = isr45;
                break;
            case 14:
                handler = isr46;
                break;
            case 15:
                handler = isr47;
                break;
        }
        set_interrupt_handler(32 + irq, handler, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT_GATE);
    }

    idtr.offset = reinterpret_cast<uint64_t>(&idt);
    load_idt(&idtr);
}

void set_interrupt_handler(uint8_t vector, void (*handler)(), uint8_t flags) {
    if (handler) create_descriptor(idt[vector], handler, 0x08, flags);
}