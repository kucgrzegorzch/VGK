CC      := gcc
LD      := ld
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m64 -mno-red-zone -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 -fno-unwind-tables -fno-asynchronous-unwind-tables -Iinclude
ASFLAGS := -m64 -fno-pic -fno-pie
LDFLAGS := -nostdlib -z max-page-size=0x1000 -T linker.ld

.PHONY: all clean iso check-tools test
all: build/vgk-kernel.elf

test: build/vgk-kernel.elf
	python3 tests/verify_kernel.py build/vgk-kernel.elf

build:
	mkdir -p build

build/entry.o: boot/entry.asm | build
	$(CC) $(ASFLAGS) -x assembler -c $< -o $@

build/kernel.o: src/kernel.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/vgk-kernel.elf: build/entry.o build/kernel.o linker.ld
	$(LD) $(LDFLAGS) -o $@ build/entry.o build/kernel.o

check-tools:
	@command -v grub-mkrescue >/dev/null || (echo "Missing tool: grub-mkrescue"; exit 1)
	@command -v xorriso >/dev/null || (echo "Missing tool: xorriso"; exit 1)
	@command -v grub-file >/dev/null || (echo "Missing tool: grub-file"; exit 1)

iso: check-tools build/vgk-kernel.elf
	mkdir -p build/iso/boot/grub
	cp build/vgk-kernel.elf build/iso/boot/vgk-kernel.elf
	cp boot/grub.cfg build/iso/boot/grub/grub.cfg
	grub-mkrescue -o build/VGK-0.1.iso build/iso

clean:
	rm -rf build
