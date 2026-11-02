# VGK 0.1 – własne jądro x86-64 (prototyp)

VGK jest eksperymentalnym projektem niezależnego systemu operacyjnego. Ten etap zawiera własny, freestanding kernel ELF64 uruchamiany przez bootloader zgodny z Multiboot2. Nie jest to jeszcze kompletny system operacyjny.

## Stan funkcji

- [x] Własny punkt wejścia jądra i przejście do trybu 64-bitowego
- [x] Podstawowe mapowanie tożsamościowe pierwszego 1 GiB pamięci (strony 2 MiB)
- [x] Sprawdzenie przez CPUID, czy procesor obsługuje long mode
- [x] Komunikat startowy na ekranie VGA oraz porcie szeregowym COM1
- [x] Statyczna kontrola ELF64 i nagłówka Multiboot2 (`make test`)
- [x] Lokalna kompilacja sprawdzona GCC i Clang (Clang może wypisać nieszkodliwe ostrzeżenie o fladze asemblera)
- [ ] Parsowanie mapy pamięci Multiboot2 i własny allocator
- [ ] IDT, wyjątki i przerwania
- [ ] Sterowniki urządzeń i VGK Guardian
- [ ] Pulpit, instalator i obsługa aplikacji
- [ ] Potwierdzony test uruchomienia w emulatorze lub na fizycznym komputerze

## Kompilacja

Wymagane: GCC lub Clang z obsługą x86-64, GNU binutils, GNU Make i Python 3.

```sh
make clean all
make test
```

## Obraz ISO

Do utworzenia BIOS-owego ISO potrzebne są dodatkowo `grub-mkrescue`, GRUB dla platformy PC, `xorriso` i `mtools`.

```sh
make iso
```

Plik wynikowy: `build/VGK-0.1.iso`.

Repozytorium zawiera workflow GitHub Actions, który po wysłaniu zmian buduje jądro, uruchamia test statyczny, tworzy ISO i publikuje je jako artefakt o nazwie `VGK-0.1-ISO`. Ten artefakt będzie dostępny w zakładce Actions po poprawnym wykonaniu workflow.

## Ważne ograniczenia prototypu

- Obecny boot path wymaga procesora x86-64 i bootloadera Multiboot2.
- Konfiguracja ISO jest przeznaczona na start BIOS/Legacy; start UEFI/GOP nie jest jeszcze zaimplementowany.
- Wczesny ekran używa klasycznej pamięci tekstowej VGA.
- Mapa stron obejmuje obecnie tylko pierwsze 1 GiB przestrzeni fizycznej.
- Zarządzanie pamięcią, sterowniki, bezpieczeństwo, Guardian, SafeCore, desktop i aplikacje nie są jeszcze zaimplementowane.
- Test `make test` sprawdza strukturę pliku, a nie uruchomienie jądra.

## Kontrola poprawki GDT (2026-11-02)

- Poprawiono deskryptor segmentu danych GDT na wariant 64-bitowego trybu zgodny z architekturą (L=0, D/B=1).
- Uproszczono flagi kompilacji pliku asemblerowego, aby Clang nie zgłaszał nieużywanego argumentu.
- Po poprawce wykonano ponownie czystą kompilację i test `make test` przy użyciu GCC oraz Clang; oba zestawy zakończyły się powodzeniem.
- Nadal wymagany jest test uruchomienia pod GRUB/QEMU; nie został wykonany w tym środowisku.
