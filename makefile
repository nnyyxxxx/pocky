CXX = g++
AS = nasm
LD = ld

ASFLAGS = -I$(BOOTLOADER_SRC)

CXXFLAGS = -m64 -std=c++14 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fstack-protector-strong \
           -I$(KERNEL_SRC) -I$(KERNEL_SRC)/hw -I$(KERNEL_SRC)/core -I$(KERNEL_SRC)/shell \
           -I$(KERNEL_SRC)/drivers -I$(KERNEL_SRC)/lib

KERNEL_SRC = kernel/src
BOOTLOADER_SRC = bootloader/src
BUILD_DIR = build

KERNEL_CPP_SRCS = $(KERNEL_SRC)/core/kernel.cpp $(KERNEL_SRC)/lib/lib.cpp $(KERNEL_SRC)/hw/io.cpp \
                  $(KERNEL_SRC)/shell/vga.cpp $(KERNEL_SRC)/shell/terminal.cpp $(KERNEL_SRC)/drivers/keyboard.cpp \
                  $(KERNEL_SRC)/hw/pic.cpp $(KERNEL_SRC)/shell/shell.cpp $(KERNEL_SRC)/hw/gdt.cpp \
                  $(KERNEL_SRC)/hw/idt.cpp

KERNEL_ASM_SRCS = $(KERNEL_SRC)/asm/entry.asm $(KERNEL_SRC)/asm/gdt.asm $(KERNEL_SRC)/asm/idt.asm \
                  $(KERNEL_SRC)/asm/isr.asm

KERNEL_CPP_OBJS = $(patsubst $(KERNEL_SRC)/%.cpp,$(BUILD_DIR)/%.o,$(KERNEL_CPP_SRCS))
KERNEL_ASM_OBJS = $(patsubst $(KERNEL_SRC)/%.asm,$(BUILD_DIR)/%_asm.o,$(KERNEL_ASM_SRCS))

KERNEL_OBJ = $(KERNEL_ASM_OBJS) $(KERNEL_CPP_OBJS)

BOOTLOADER_BIN = $(BUILD_DIR)/boot.bin
BOOTLOADER_STAGE2_BIN = $(BUILD_DIR)/stage2.bin
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
OS_IMAGE = $(BUILD_DIR)/os.img

all: format clean $(OS_IMAGE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/core
	mkdir -p $(BUILD_DIR)/lib
	mkdir -p $(BUILD_DIR)/hw
	mkdir -p $(BUILD_DIR)/shell
	mkdir -p $(BUILD_DIR)/drivers
	mkdir -p $(BUILD_DIR)/asm

$(BUILD_DIR)/%.o: $(KERNEL_SRC)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%_asm.o: $(KERNEL_SRC)/%.asm | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -f elf64 $< -o $@

$(BOOTLOADER_BIN): $(BOOTLOADER_SRC)/stages/boot.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -f bin $(BOOTLOADER_SRC)/stages/boot.asm -o $@

$(BOOTLOADER_STAGE2_BIN): $(BOOTLOADER_SRC)/stages/stage2.asm $(BOOTLOADER_SRC)/memory/gdt.asm \
                         $(BOOTLOADER_SRC)/mode/pm_switch.asm $(BOOTLOADER_SRC)/memory/gdt64.asm \
                         $(BOOTLOADER_SRC)/mode/long_mode.asm $(BOOTLOADER_SRC)/mode/lm_switch.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -f bin $(BOOTLOADER_SRC)/stages/stage2.asm -o $@

$(KERNEL_BIN): $(KERNEL_OBJ) kernel/linker.ld | $(BUILD_DIR)
	$(LD) -T kernel/linker.ld -m elf_x86_64 -nostdlib $(KERNEL_OBJ) -o $(BUILD_DIR)/kernel_elf.bin
	objcopy -O binary $(BUILD_DIR)/kernel_elf.bin $@

$(OS_IMAGE): $(BOOTLOADER_BIN) $(BOOTLOADER_STAGE2_BIN) $(KERNEL_BIN)
	dd if=/dev/zero of=$@ bs=512 count=2880
	dd if=$(BOOTLOADER_BIN) of=$@ conv=notrunc
	dd if=$(BOOTLOADER_STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=8 conv=notrunc

run: $(OS_IMAGE)
	@echo "Killing any existing QEMU processes..."
	-pkill -f qemu-system-x86_64 || true
	qemu-system-x86_64 -drive file=build/os.img,format=raw,if=floppy -display vnc=127.0.0.1:0 &
	@echo "Waiting for QEMU to start..."
	@sleep 1
	@echo "Launching VNC viewer..."
	vncviewer 127.0.0.1:0 || gvncviewer 127.0.0.1:0 || vinagre 127.0.0.1:0 || open vnc://127.0.0.1:0 || echo "Could not find a VNC viewer. Please connect manually to 127.0.0.1:5900"

clean:
	rm -rf $(BUILD_DIR)

format:
	clang-format -i $(KERNEL_CPP_SRCS)

.PHONY: all run clean format