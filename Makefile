# VGK kernel and native UEFI Stage 0 build targets.
CC      := gcc
LD      := ld
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m64 -mno-red-zone -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 -fno-unwind-tables -fno-asynchronous-unwind-tables -Iinclude
ASFLAGS := -m64 -fno-pic -fno-pie
LDFLAGS := -nostdlib -z max-page-size=0x1000 -T linker.ld
CLANG   := clang
LLD_LINK := lld-link
UEFI_CFLAGS := --target=x86_64-pc-win32-coff -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fshort-wchar -fno-stack-protector -fno-builtin -mno-red-zone
UEFI_LDFLAGS := /subsystem:efi_application /entry:efi_main /nodefaultlib /machine:x64

.PHONY: all clean iso check-tools test uefi uefi-test security-test sha256-test integrity-test ed25519-test manifest-test
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

build/bootx64.obj: boot/uefi/bootx64.c | build
	$(CLANG) $(UEFI_CFLAGS) -c $< -o $@

build/BOOTX64.EFI: build/bootx64.obj
	$(LLD_LINK) $(UEFI_LDFLAGS) /out:$@ $<

uefi: build/BOOTX64.EFI

uefi-test: build/BOOTX64.EFI
	python3 tests/check_uefi_image.py build/BOOTX64.EFI

security-test: sha256-test integrity-test ed25519-test manifest-test
	python3 -m unittest discover -s tests -p 'test_sprawdz_wydanie.py' -v

sha256-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iboot/crypto boot/crypto/sha256.c tests/test_sha256.c -o build/test_sha256
	./build/test_sha256

integrity-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iboot/crypto boot/crypto/sha256.c boot/crypto/integrity.c tests/test_integrity.c -o build/test_integrity
	./build/test_integrity

ed25519-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iboot/crypto -Iboot/crypto/monocypher boot/crypto/monocypher/monocypher.c boot/crypto/monocypher/monocypher-ed25519.c boot/crypto/ed25519.c tests/test_ed25519.c -o build/test_ed25519
	./build/test_ed25519

manifest-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iboot/crypto -Iboot/crypto/monocypher boot/crypto/sha256.c boot/crypto/monocypher/monocypher.c boot/crypto/monocypher/monocypher-ed25519.c boot/crypto/ed25519.c boot/crypto/manifest.c tests/test_manifest.c -o build/test_manifest
	./build/test_manifest

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
