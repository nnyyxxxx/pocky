KERNEL_SRC = kernel/src
BUILD_DIR = build
IMAGE_DIR = image

default: build

build: format
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && make

all: clean build

format:
	@clang-format -i $(shell find $(KERNEL_SRC) -name "*.cpp" -o -name "*.hpp") > /dev/null 2>&1

image:
	@mkdir -p $(IMAGE_DIR)
	@if [ ! -f $(IMAGE_DIR)/fat32.img ]; then \
		dd if=/dev/zero of=$(IMAGE_DIR)/fat32.img bs=1M count=64; \
		mkfs.fat -F 32 $(IMAGE_DIR)/fat32.img; \
	fi

run:
	@if [ ! -f $(IMAGE_DIR)/fat32.img ]; then \
		echo "No file system image found, aborting..."; \
		echo "Please run 'make image' to create an image.\n"; \
		exit 1; \
	fi

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
	@grub-mkrescue -o $(BUILD_DIR)/os.iso $(BUILD_DIR)/iso 2>/dev/null
	-pkill -f qemu-system-x86_64 || true
	qemu-system-x86_64 -cdrom $(BUILD_DIR)/os.iso -drive file=$(IMAGE_DIR)/fat32.img,format=raw -boot order=d -m 512M -display gtk -no-reboot -serial stdio -smp cores=2,threads=1

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all build run clean format image