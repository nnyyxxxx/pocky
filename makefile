KERNEL_SRC = kernel/src
BUILD_DIR = build

default: build

build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && make

all: clean build

format:
	clang-format -i $(shell find $(KERNEL_SRC) -name "*.cpp")

run:
	@echo "Setting up GRUB bootable ISO..."
	@mkdir -p $(BUILD_DIR)/iso/boot/grub
	@echo "set timeout=10" > $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "set default=0" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "menuentry \"Pocky\" {" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "    insmod all_video" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "    insmod vbe" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "    insmod gfxterm" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "    set gfxpayload=keep" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	@echo "    echo \"Loading Pocky...\"" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
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

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all build run clean format