CXX = g++
AS = nasm
LD = ld

CXXFLAGS = -m64 -std=c++14 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fstack-protector-strong \
           -I$(KERNEL_SRC) -I$(KERNEL_SRC)/hw -I$(KERNEL_SRC)/core -I$(KERNEL_SRC)/shell \
           -I$(KERNEL_SRC)/drivers -I$(KERNEL_SRC)/lib -I$(KERNEL_SRC)/memory -I$(KERNEL_SRC)/fs

KERNEL_SRC = kernel/src
BUILD_DIR = build

KERNEL_CPP_SRCS = $(KERNEL_SRC)/core/kernel.cpp $(KERNEL_SRC)/lib/lib.cpp $(KERNEL_SRC)/hw/io.cpp \
                  $(KERNEL_SRC)/shell/vga.cpp $(KERNEL_SRC)/shell/terminal.cpp $(KERNEL_SRC)/drivers/keyboard.cpp \
                  $(KERNEL_SRC)/drivers/mouse.cpp $(KERNEL_SRC)/hw/pic.cpp $(KERNEL_SRC)/shell/shell.cpp $(KERNEL_SRC)/hw/gdt.cpp \
                  $(KERNEL_SRC)/hw/idt.cpp $(KERNEL_SRC)/memory/physical_memory.cpp \
                  $(KERNEL_SRC)/memory/virtual_memory.cpp $(KERNEL_SRC)/memory/heap.cpp \
                  $(KERNEL_SRC)/core/init.cpp $(KERNEL_SRC)/core/elf.cpp $(KERNEL_SRC)/core/dynamic_linker.cpp \
                  $(KERNEL_SRC)/lib/printf.cpp $(KERNEL_SRC)/fs/filesystem.cpp $(KERNEL_SRC)/hw/timer.cpp \
                  $(KERNEL_SRC)/shell/editor.cpp $(KERNEL_SRC)/core/multiboot2.cpp $(KERNEL_SRC)/shell/graphics.cpp

KERNEL_ASM_SRCS = $(KERNEL_SRC)/asm/entry.asm $(KERNEL_SRC)/asm/gdt.asm $(KERNEL_SRC)/asm/idt.asm \
                  $(KERNEL_SRC)/asm/isr.asm $(KERNEL_SRC)/asm/vm.asm $(KERNEL_SRC)/asm/timer.asm \
                  $(KERNEL_SRC)/asm/mouse.asm

KERNEL_CPP_OBJS = $(patsubst $(KERNEL_SRC)/%.cpp,$(BUILD_DIR)/%.o,$(KERNEL_CPP_SRCS))
KERNEL_ASM_OBJS = $(patsubst $(KERNEL_SRC)/asm/%.asm,$(BUILD_DIR)/asm/%_asm.o,$(KERNEL_ASM_SRCS))

KERNEL_OBJ = $(KERNEL_ASM_OBJS) $(KERNEL_CPP_OBJS)
KERNEL_BIN = $(BUILD_DIR)/kernel.bin

all: format clean $(KERNEL_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/core
	mkdir -p $(BUILD_DIR)/lib
	mkdir -p $(BUILD_DIR)/hw
	mkdir -p $(BUILD_DIR)/shell
	mkdir -p $(BUILD_DIR)/drivers
	mkdir -p $(BUILD_DIR)/asm
	mkdir -p $(BUILD_DIR)/memory
	mkdir -p $(BUILD_DIR)/fs

$(BUILD_DIR)/%.o: $(KERNEL_SRC)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%_asm.o: $(KERNEL_SRC)/%.asm | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(AS) -f elf64 $< -o $@

$(KERNEL_BIN): $(KERNEL_OBJ) kernel/linker.ld | $(BUILD_DIR)
	$(LD) -T kernel/linker.ld -m elf_x86_64 -nostdlib $(KERNEL_OBJ) -o $(BUILD_DIR)/kernel_elf.bin
	objcopy -O binary $(BUILD_DIR)/kernel_elf.bin $@

run: $(KERNEL_BIN)
	@echo "Setting up GRUB bootable ISO..."
	@mkdir -p $(BUILD_DIR)/iso/boot/grub
	@echo "set timeout=10" > $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "set default=0" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "menuentry \"Kernel\" {" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "    insmod all_video" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "    insmod vbe" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "    insmod gfxterm" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "    set gfxpayload=keep" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "    echo \"Loading multiboot2 kernel...\"" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "    multiboot2 /boot/kernel.bin" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "    boot" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "}" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@cp $(BUILD_DIR)/kernel_elf.bin $(BUILD_DIR)/iso/boot/kernel.bin
	@echo "Creating bootable ISO with GRUB..."
	@grub-mkrescue -o $(BUILD_DIR)/os.iso $(BUILD_DIR)/iso 2>/dev/null
	@echo "Launching QEMU with GRUB..."
	@echo "Killing any existing QEMU processes..."
	-pkill -f qemu-system-x86_64 || true
	qemu-system-x86_64 -cdrom $(BUILD_DIR)/os.iso -m 512M -display gtk -no-reboot -serial stdio

run_vnc: $(KERNEL_BIN)
	@echo "Killing any existing QEMU processes..."
	-pkill -f qemu-system-x86_64 || true
	qemu-system-x86_64 -drive file=$(KERNEL_BIN),format=raw -display vnc=127.0.0.1:0 &
	@echo "Waiting for QEMU to start..."
	@sleep 1
	@echo "Launching VNC viewer..."
	vncviewer 127.0.0.1:0 || gvncviewer 127.0.0.1:0 || vinagre 127.0.0.1:0 || open vnc://127.0.0.1:0 || echo "Could not find a VNC viewer. Please connect manually to 127.0.0.1:5900"

clean:
	rm -rf $(BUILD_DIR)

format:
	clang-format -i $(KERNEL_CPP_SRCS)

.PHONY: all run run_vnc clean format