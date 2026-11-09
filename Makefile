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
CRYPTO_DIR := boot/crypto
MONOCYPHER_DIR := $(CRYPTO_DIR)/monocypher

.PHONY: all clean iso check-tools test native-kernel native-kernel-test uefi uefi-test uefi-keyed-compile-test uefi-keyed-link-test uefi-qemu-image uefi-abi-test uefi-services-abi-test uefi-reader-test uefi-manifest-test security-test keygen-test vgkm-test sha256-test integrity-test ed25519-test manifest-test boot-info-test uefi-memory-map-test uefi-elf-test uefi-elf-allocator-test memory-test cpu-features-test interrupts-test exception-test framebuffer-test pci-test
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

# Niezalezny obraz jadra VGK – bez naglowka GRUB/Multiboot2.
build/entry_native.o: boot/entry_native.S | build
	$(CC) -m64 -ffreestanding -fno-pic -fno-pie -c $< -o $@

build/exception_stubs_native.o: boot/exception_stubs.S | build
	$(CC) -m64 -ffreestanding -fno-pic -fno-pie -c $< -o $@

build/kernel_native.o: src/kernel_native.c include/vgk_boot_info.h include/vgk_memory.h include/vgk_cpu_features.h | build
	$(CC) $(CFLAGS) -mcmodel=small -c $< -o $@

build/cpu_features_native.o: src/cpu_features.c include/vgk_cpu_features.h | build
	$(CC) $(CFLAGS) -mcmodel=small -c $< -o $@

build/memory_native.o: src/memory.c include/vgk_memory.h include/vgk_boot_info.h | build
	$(CC) $(CFLAGS) -mcmodel=small -c $< -o $@

build/vgk-kernel-native.elf: build/entry_native.o build/exception_stubs_native.o build/kernel_native.o build/memory_native.o build/cpu_features_native.o linker-native.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T linker-native.ld -o $@ build/entry_native.o build/exception_stubs_native.o build/kernel_native.o build/memory_native.o build/cpu_features_native.o

native-kernel: build/vgk-kernel-native.elf

native-kernel-test: build/vgk-kernel-native.elf
	python3 tests/check_native_kernel.py build/vgk-kernel-native.elf
	python3 tests/check_exception_stubs.py build/vgk-kernel-native.elf

memory-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude src/memory.c tests/test_memory.c -o build/test_memory
	./build/test_memory

cpu-features-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude src/cpu_features.c tests/test_cpu_features.c -o build/test_cpu_features
	./build/test_cpu_features

interrupts-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude src/interrupts.c tests/test_interrupts.c -o build/test_interrupts
	./build/test_interrupts

exception-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude src/exception.c tests/test_exception.c -o build/test_exception
	./build/test_exception

build/bootx64.obj: boot/uefi/bootx64.c boot/uefi/uefi_graphics.h include/vgk_framebuffer.h boot/uefi/uefi_file.h boot/uefi/uefi_services.h boot/uefi/uefi_release_verify.h boot/crypto/manifest.h | build
	$(CLANG) $(UEFI_CFLAGS) -Iinclude -Iboot/uefi -Iboot/crypto -c $< -o $@

# Most UEFI functions use Microsoft x64 ABI; this bridge prepares the native
# kernel entry register contract (System V: VGK_BOOT_INFO in RDI).
build/uefi_kernel_entry.obj: boot/uefi/uefi_kernel_entry.S boot/uefi/uefi_kernel_entry.h include/vgk_boot_info.h | build
	$(CLANG) --target=x86_64-pc-win32-coff -mno-red-zone -c boot/uefi/uefi_kernel_entry.S -o $@

build/uefi_file_reader.obj: boot/uefi/uefi_file_reader.c boot/uefi/uefi_file_reader.h boot/uefi/uefi_file.h boot/crypto/manifest.h boot/crypto/integrity.h | build
	$(CLANG) $(UEFI_CFLAGS) -Iboot/uefi -Iboot/crypto -c $< -o $@

build/elf_loader.obj: boot/uefi/elf_loader.c boot/uefi/elf_loader.h | build
	$(CLANG) $(UEFI_CFLAGS) -Iboot/uefi -c $< -o $@

build/uefi_elf_allocator.obj: boot/uefi/uefi_elf_allocator.c boot/uefi/uefi_elf_allocator.h boot/uefi/elf_loader.h boot/uefi/uefi_services.h | build
	$(CLANG) $(UEFI_CFLAGS) -Iboot/uefi -c $< -o $@

build/uefi_memory_map.obj: boot/uefi/uefi_memory_map.c boot/uefi/uefi_memory_map.h boot/uefi/uefi_services.h include/vgk_boot_info.h | build
	$(CLANG) $(UEFI_CFLAGS) -Iinclude -Iboot/uefi -c $< -o $@

build/uefi_release_verify.obj: boot/uefi/uefi_release_verify.c boot/uefi/uefi_release_verify.h boot/uefi/uefi_file_reader.h | build
	$(CLANG) $(UEFI_CFLAGS) -Iboot/uefi -Iboot/crypto -c $< -o $@

build/sha256.obj: $(CRYPTO_DIR)/sha256.c | build
	$(CLANG) $(UEFI_CFLAGS) -I$(CRYPTO_DIR) -c $< -o $@

build/integrity.obj: $(CRYPTO_DIR)/integrity.c | build
	$(CLANG) $(UEFI_CFLAGS) -I$(CRYPTO_DIR) -c $< -o $@

build/manifest.obj: $(CRYPTO_DIR)/manifest.c | build
	$(CLANG) $(UEFI_CFLAGS) -I$(CRYPTO_DIR) -I$(MONOCYPHER_DIR) -c $< -o $@

build/ed25519.obj: $(CRYPTO_DIR)/ed25519.c | build
	$(CLANG) $(UEFI_CFLAGS) -I$(CRYPTO_DIR) -I$(MONOCYPHER_DIR) -c $< -o $@

build/monocypher.obj: $(MONOCYPHER_DIR)/monocypher.c | build
	$(CLANG) $(UEFI_CFLAGS) -I$(MONOCYPHER_DIR) -c $< -o $@

build/monocypher_ed25519.obj: $(MONOCYPHER_DIR)/monocypher-ed25519.c | build
	$(CLANG) $(UEFI_CFLAGS) -I$(MONOCYPHER_DIR) -c $< -o $@

build/BOOTX64.EFI: build/bootx64.obj build/uefi_kernel_entry.obj build/elf_loader.obj build/uefi_elf_allocator.obj build/uefi_memory_map.obj build/uefi_file_reader.obj build/uefi_release_verify.obj build/sha256.obj build/integrity.obj build/manifest.obj build/ed25519.obj build/monocypher.obj build/monocypher_ed25519.obj
	$(LLD_LINK) $(UEFI_LDFLAGS) /out:$@ $^

uefi: build/BOOTX64.EFI

uefi-test: build/BOOTX64.EFI
	python3 tests/check_uefi_image.py build/BOOTX64.EFI

# Compile and link the trusted-key path with a test-only key.
# This image is for CI only and is never published as a release artifact.
build/bootx64-keyed-test.obj: boot/uefi/bootx64.c boot/uefi/uefi_graphics.h include/vgk_framebuffer.h boot/uefi/uefi_file.h boot/uefi/uefi_services.h boot/uefi/uefi_release_verify.h boot/uefi/uefi_file_reader.h boot/uefi/elf_loader.h boot/uefi/uefi_elf_allocator.h boot/uefi/uefi_memory_map.h boot/uefi/uefi_kernel_entry.h include/vgk_boot_info.h tests/fixtures/vgk_trusted_public_key.h | build
	$(CLANG) $(UEFI_CFLAGS) -Itests/fixtures -Iinclude -Iboot/uefi -Iboot/crypto -c $< -o $@

build/BOOTX64-KEYED-TEST.EFI: build/bootx64-keyed-test.obj build/uefi_kernel_entry.obj build/elf_loader.obj build/uefi_elf_allocator.obj build/uefi_memory_map.obj build/uefi_file_reader.obj build/uefi_release_verify.obj build/sha256.obj build/integrity.obj build/manifest.obj build/ed25519.obj build/monocypher.obj build/monocypher_ed25519.obj
	$(LLD_LINK) $(UEFI_LDFLAGS) /out:$@ $^

# QEMU image uses a fresh ephemeral key generated by the CI job.
build/bootx64-qemu-test.obj: boot/uefi/bootx64.c boot/uefi/uefi_graphics.h include/vgk_framebuffer.h boot/uefi/uefi_file.h boot/uefi/uefi_services.h boot/uefi/uefi_release_verify.h boot/uefi/uefi_file_reader.h boot/uefi/elf_loader.h boot/uefi/uefi_elf_allocator.h boot/uefi/uefi_memory_map.h boot/uefi/uefi_kernel_entry.h include/vgk_boot_info.h build/vgk_trusted_public_key.h | build
	$(CLANG) $(UEFI_CFLAGS) -Ibuild -Iinclude -Iboot/uefi -Iboot/crypto -c $< -o $@

build/BOOTX64-QEMU-TEST.EFI: build/bootx64-qemu-test.obj build/uefi_kernel_entry.obj build/elf_loader.obj build/uefi_elf_allocator.obj build/uefi_memory_map.obj build/uefi_file_reader.obj build/uefi_release_verify.obj build/sha256.obj build/integrity.obj build/manifest.obj build/ed25519.obj build/monocypher.obj build/monocypher_ed25519.obj
	$(LLD_LINK) $(UEFI_LDFLAGS) /out:$@ $^

uefi-qemu-image: build/BOOTX64-QEMU-TEST.EFI

uefi-keyed-compile-test: build/bootx64-keyed-test.obj

uefi-keyed-link-test: build/BOOTX64-KEYED-TEST.EFI
	python3 tests/check_uefi_image.py $<

uefi-abi-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iboot/uefi tests/test_uefi_file_abi.c -o build/test_uefi_file_abi
	./build/test_uefi_file_abi

boot-info-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/test_boot_info.c -o build/test_boot_info
	./build/test_boot_info

uefi-services-abi-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iboot/uefi tests/test_uefi_services_abi.c -o build/test_uefi_services_abi
	./build/test_uefi_services_abi

uefi-elf-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iboot/uefi boot/uefi/elf_loader.c tests/test_elf_loader.c -o build/test_elf_loader
	./build/test_elf_loader

uefi-memory-map-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude -Iboot/uefi boot/uefi/uefi_memory_map.c tests/test_uefi_memory_map.c -o build/test_uefi_memory_map
	./build/test_uefi_memory_map

uefi-reader-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iboot/uefi -Iboot/crypto tests/test_uefi_file_reader.c boot/uefi/uefi_file_reader.c -o build/test_uefi_file_reader
	./build/test_uefi_file_reader

uefi-manifest-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iboot/uefi -Iboot/crypto -Iboot/crypto/monocypher boot/crypto/sha256.c boot/crypto/integrity.c boot/crypto/manifest.c boot/crypto/ed25519.c boot/crypto/monocypher/monocypher.c boot/crypto/monocypher/monocypher-ed25519.c boot/uefi/uefi_file_reader.c boot/uefi/uefi_release_verify.c tests/test_uefi_manifest_integration.c -o build/test_uefi_manifest_integration
	./build/test_uefi_manifest_integration

vgkm-test: | build
	PYTHONPATH=tools python3 -m unittest discover -s tests -p 'test_vgkm.py' -v

keygen-test: | build
	python3 -m unittest discover -s tests -p 'test_klucz_zaufania.py' -v

security-test: keygen-test vgkm-test sha256-test integrity-test ed25519-test manifest-test uefi-manifest-test uefi-memory-map-test uefi-elf-test uefi-elf-allocator-test framebuffer-test
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
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iboot/crypto -Iboot/crypto/monocypher boot/crypto/sha256.c boot/crypto/integrity.c boot/crypto/monocypher/monocypher.c boot/crypto/monocypher/monocypher-ed25519.c boot/crypto/ed25519.c boot/crypto/manifest.c tests/test_manifest.c -o build/test_manifest
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

framebuffer-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/test_framebuffer.c -o build/test_framebuffer
	./build/test_framebuffer

pci-test: | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude src/pci.c tests/test_pci.c -o build/test_pci
	./build/test_pci
