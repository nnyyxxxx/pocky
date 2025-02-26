CXX = g++
AS = nasm
LD = ld

CXXFLAGS = -m64 -std=c++14 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti

KERNEL_SRC = kernel/src
BOOTLOADER_SRC = bootloader/src
BUILD_DIR = build

KERNEL_CPP_SRCS = $(KERNEL_SRC)/kernel.cpp $(KERNEL_SRC)/lib.cpp $(KERNEL_SRC)/io.cpp \
                  $(KERNEL_SRC)/vga.cpp $(KERNEL_SRC)/terminal.cpp $(KERNEL_SRC)/keyboard.cpp \
                  $(KERNEL_SRC)/pic.cpp $(KERNEL_SRC)/shell.cpp

KERNEL_ASM_SRCS = $(KERNEL_SRC)/entry.asm

KERNEL_CPP_OBJS = $(patsubst $(KERNEL_SRC)/%.cpp,$(BUILD_DIR)/%.o,$(KERNEL_CPP_SRCS))
KERNEL_ASM_OBJS = $(patsubst $(KERNEL_SRC)/%.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SRCS))

KERNEL_OBJ = $(KERNEL_ASM_OBJS) $(KERNEL_CPP_OBJS)

BOOTLOADER_BIN = $(BUILD_DIR)/boot.bin
BOOTLOADER_STAGE2_BIN = $(BUILD_DIR)/stage2.bin
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
OS_IMAGE = $(BUILD_DIR)/os.img

all: clean $(OS_IMAGE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(KERNEL_SRC)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_SRC)/%.asm | $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BOOTLOADER_BIN): $(BOOTLOADER_SRC)/boot.asm | $(BUILD_DIR)
	$(AS) -f bin $(BOOTLOADER_SRC)/boot.asm -o $@

$(BOOTLOADER_STAGE2_BIN): $(BOOTLOADER_SRC)/stage2.asm $(BOOTLOADER_SRC)/gdt.asm $(BOOTLOADER_SRC)/pm_switch.asm $(BOOTLOADER_SRC)/gdt64.asm $(BOOTLOADER_SRC)/long_mode.asm $(BOOTLOADER_SRC)/lm_switch.asm | $(BUILD_DIR)
	$(AS) -f bin $(BOOTLOADER_SRC)/stage2.asm -o $@

$(KERNEL_BIN): $(KERNEL_OBJ) kernel/linker.ld | $(BUILD_DIR)
	$(LD) -T kernel/linker.ld -m elf_x86_64 -nostdlib $(KERNEL_OBJ) -o $(BUILD_DIR)/kernel_elf.bin
	objcopy -O binary $(BUILD_DIR)/kernel_elf.bin $@

$(OS_IMAGE): $(BOOTLOADER_BIN) $(BOOTLOADER_STAGE2_BIN) $(KERNEL_BIN) | $(BUILD_DIR)
	dd if=/dev/zero of=$(OS_IMAGE) bs=512 count=2880
	dd if=$(BOOTLOADER_BIN) of=$(OS_IMAGE) conv=notrunc
	dd if=$(BOOTLOADER_STAGE2_BIN) of=$(OS_IMAGE) seek=1 conv=notrunc bs=512
	dd if=$(KERNEL_BIN) of=$(OS_IMAGE) seek=9 conv=notrunc bs=512

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

.PHONY: all run clean