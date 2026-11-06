# VGK Native Boot — architecture

## Direction
The intended VGK boot path is original VGK code. GRUB and Multiboot2 are not components of that path. UEFI and legacy BIOS are firmware interfaces and specifications; using them does not make VGK a Linux-, Windows-, or Apple-based operating system.

The old GRUB-based 0.1 prototype is historical material during development. It must not be invoked by the new build or boot path.

## Stages
1. UEFI Stage 0: a VGK-authored PE32+ EFI application is loaded directly by UEFI firmware and prints a diagnostic banner.
2. UEFI loader: discover the VGK system partition, open and validate the kernel image, obtain the memory map and graphics information, and build a versioned VGK boot-information block.
3. Kernel handoff: call ExitBootServices with a current memory-map key, then transfer control to the VGK kernel using the documented VGK ABI.
4. Legacy BIOS loader: a separate VGK-authored 16-bit boot path establishes CPU state and provides the same kernel handoff ABI.
5. Unified media: create media selectable by UEFI or legacy BIOS only after both paths have independent tests.

## Planned kernel handoff structure
The versioned structure will describe the protocol version and size, memory map entries, framebuffer address/dimensions/stride/pixel format, ACPI table pointer when supplied by firmware, kernel placement, and boot options.

The kernel must not depend on Multiboot2 tags or GRUB services. The current 0.1 kernel cannot be called through this interface until its entry code, linker layout, and memory initialization are adapted and tested.

## Stage 0 implementation
The file boot/uefi/bootx64.c is a small x86-64 EFI application entry point. It uses the UEFI system-table console protocol to print a diagnostic banner. It is built as a PE32+ EFI application using Clang's Windows COFF target and LLD's EFI application subsystem.

Build and validate it with:

    make clean uefi
    make uefi-test

The output is build/BOOTX64.EFI. The image is intended to be copied to a FAT-formatted removable-media EFI path EFI/BOOT/BOOTX64.EFI for firmware testing.

## Current status and limits
Stage 0 is only a firmware-entry proof. It does not load the kernel, exit firmware boot services, implement BIOS boot, or produce a bootable VGK OS. A successful CI build proves only that the EFI executable is structurally valid. QEMU/OVMF and physical-machine tests remain separate milestones.

BOOTX64.EFI is the conventional removable-media UEFI filename, not a third-party bootloader.

## ABI layout safeguards

The Stage 0 source now contains compile-time assertions for the UEFI
`CHAR16` width, EFI table-header size, key `EFI_SYSTEM_TABLE` offsets and
size, and the text-output protocol function offset. These checks catch
accidental C structure-layout changes during compilation. They do not prove
that firmware execution works; that still requires a successful build and
QEMU/OVMF or physical-firmware test.


## Pierwszy odczyt z nośnika UEFI

Stage 0 pobiera teraz protokół załadowanego obrazu, z niego uchwyt urządzenia,
a następnie protokół systemu plików UEFI. Otwiera główny katalog woluminu
i odczytuje dwa bajty z własnego pliku `\\EFI\\BOOT\\BOOTX64.EFI`.
Sprawdza sygnaturę `MZ` i zamyka oba uchwyty plików.

To jest pierwszy rzeczywisty odczyt pliku z woluminu, a nie tylko deklaracja
interfejsu. Testuje ścieżkę dostępu do nośnika, ale **nie jest jeszcze
sprawdzaniem autentyczności**: sygnatura MZ potwierdza jedynie format pliku.
Weryfikacja podpisu wydania, odczyt jądra VGK, jego bezpieczne umieszczenie
w pamięci i przekazanie mu sterowania pozostają kolejnymi etapami.

Testy układu struktur usług UEFI są uruchamiane osobno w automatycznym
sprawdzaniu. Pełny test działania tej ścieżki wymaga jeszcze uruchomienia
obrazu w maszynie wirtualnej z oprogramowaniem UEFI oraz testu na sprzęcie.


## Adapter odczytu plików UEFI – stan po testach

Dodano `boot/uefi/uefi_file_reader.c` i plik nagłówkowy. Adapter łączy funkcje otwierania i zamykania plików UEFI z interfejsem odczytu wymaganym przez mechanizm sprawdzania integralności. Bezpieczne ścieżki względne są zamieniane na ścieżki UEFI, a próby użycia elementów `..`, pustych elementów lub niedozwolonych znaków są odrzucane. Odczyt wykorzystuje `SetPosition` i `Read`, dzięki czemu może pobierać kolejne fragmenty pliku.

Testy z atrapą interfejsu firmware sprawdzają konwersję ścieżki, odczyt z przesunięcia, krótkie odczyty, zamknięcie pliku oraz odrzucenie próby wyjścia poza katalog. Osobny test łączy adapter z podpisanym wykazem VGKM i SHA-256: poprawny plik przechodzi kontrolę, a zmieniona zawartość zostaje wykryta. Testy są uruchamiane przez `make uefi-reader-test` i `make uefi-manifest-test`, a także w automatycznym sprawdzaniu repozytorium.

**Ważne ograniczenie:** adapter jest dołączany do programu EFI i przechodzi testy z atrapą firmware, ale funkcja `efi_main` nie uruchamia jeszcze pełnej weryfikacji wykazu. Nie ma jeszcze zaufanego klucza publicznego wydawcy w programie startowym. Aktualny komunikat o odczycie woluminu sprawdza tylko dwa bajty sygnatury pliku EFI. Nie wolno więc jeszcze traktować tej ścieżki jako aktywnej ochrony jądra przed podmianą.
