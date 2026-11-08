# VGK 0.1 – własne jądro x86-64 (prototyp)

VGK jest eksperymentalnym projektem niezależnego systemu operacyjnego. Ten etap zawiera własny, freestanding kernel ELF64. Historyczny prototyp 0.1 był uruchamiany przez bootloader zgodny z Multiboot2; docelowy mechanizm uruchamiania VGK jest rozwijany niezależnie. Nie jest to jeszcze kompletny system operacyjny.

## Wymagania użytkowe

Zebrane wymagania dotyczące bezpieczeństwa, instalowania programów, podstawowych aplikacji, kopii zapasowych, pulpitu i trybu awaryjnego znajdują się w [planie wymagań użytkowych VGK](docs/WYMAGANIA_UZYTKOWE_VGK.md). Dokument opisuje cele przyszłych etapów, a nie funkcje już dostępne. Szczegółowy projekt odseparowanego środowiska, terminala naprawczego, pomocy lokalnej i internetowej, migawek oraz przywracania znajduje się w [projekcie trybu awaryjnego](docs/TRYB_AWARYJNY.md).

## Stan funkcji

- [x] Własny punkt wejścia jądra i przejście do trybu 64-bitowego
- [x] Podstawowe mapowanie tożsamościowe pierwszego 1 GiB pamięci (strony 2 MiB)
- [x] Sprawdzenie przez CPUID, czy procesor obsługuje long mode
- [x] Komunikat startowy na ekranie VGA oraz porcie szeregowym COM1
- [x] Statyczna kontrola ELF64 i nagłówka Multiboot2 (`make test`)
- [x] Lokalna kompilacja sprawdzona GCC i Clang (Clang może wypisać nieszkodliwe ostrzeżenie o fladze asemblera)
- [ ] Parsowanie mapy pamięci i własny allocator
- [ ] IDT, wyjątki i przerwania
- [ ] Sterowniki urządzeń
- [ ] Inteligentny Opiekun Systemu – planowany moduł monitorowania, diagnozowania i podejmowania bezpiecznych działań naprawczych
- [ ] Pulpit, instalator, aplikacje i narzędzie do tworzenia kopii zapasowych
- [ ] Potwierdzony test uruchomienia w emulatorze lub na fizycznym komputerze

## Inteligentny Opiekun Systemu

Inteligentny Opiekun Systemu będzie odrębnym modułem VGK. Jego zadaniem będzie:
- kontrolowanie podstawowych elementów systemu i wykrywanie nieprawidłowości;
- diagnozowanie problemów z jądrem, pamięcią i sterownikami;
- zapisywanie raportu diagnostycznego;
- podejmowanie napraw automatycznych wyłącznie wtedy, gdy można je wykonać bezpiecznie;
- informowanie użytkownika o wykrytych problemach i wykonanych działaniach.

Poważniejsze przywracanie systemu pozostaje zadaniem trybu awaryjnego. Moduł jest na etapie projektu; powyższe funkcje nie są jeszcze zaimplementowane ani przetestowane.

## Kompilacja

Wymagane: GCC lub Clang z obsługą x86-64, GNU binutils, GNU Make i Python 3.

```sh
make clean all
make test
```

## Obraz ISO

Stara ścieżka tworzenia prototypowego BIOS-owego ISO korzysta z GRUB/Multiboot2. Nie jest to docelowy mechanizm uruchamiania VGK.

```sh
make iso
```

Plik wynikowy: `build/VGK-0.1.iso`.

Repozytorium zawiera workflow GitHub Actions, który buduje historyczny prototyp, uruchamia test statyczny, tworzy ISO i publikuje je jako artefakt o nazwie `VGK-0.1-ISO`. Ten artefakt będzie dostępny w zakładce Actions po poprawnym wykonaniu workflow.

## Ważne ograniczenia prototypu

- Historyczny boot path wymaga procesora x86-64 i bootloadera Multiboot2.
- Konfiguracja starego ISO jest przeznaczona na start BIOS/Legacy; start UEFI/GOP nie jest jeszcze potwierdzony.
- Wczesny ekran używa klasycznej pamięci tekstowej VGA.
- Mapa stron obejmuje obecnie tylko pierwsze 1 GiB przestrzeni fizycznej.
- Zarządzanie pamięcią, sterowniki, bezpieczeństwo, Inteligentny Opiekun Systemu, SafeCore, pulpit i aplikacje nie są jeszcze zaimplementowane.
- Test `make test` sprawdza strukturę pliku, a nie uruchomienie jądra.

## Kontrola poprawki GDT (2026-11-02)

- Poprawiono deskryptor segmentu danych GDT na wariant 64-bitowego trybu zgodny z architekturą (L=0, D/B=1).
- Uproszczono flagi kompilacji pliku asemblerowego, aby Clang nie zgłaszał nieużywanego argumentu.
- Po poprawce wykonano ponownie czystą kompilację i test `make test` przy użyciu GCC oraz Clang; oba zestawy zakończyły się powodzeniem.
- Nadal wymagany jest test uruchomienia pod GRUB/QEMU; nie został wykonany w tym środowisku.

## Mechanizm Bezpieczeństwa i Zaufania

Opisano go w [dokumentacji bezpieczeństwa](docs/BEZPIECZENSTWO.md). Repozytorium zawiera pomocnicze narzędzia do przygotowania i sprawdzania podpisanego wykazu plików (`tools/przygotuj_wydanie.py`, `tools/sprawdz_wydanie.py`); testy uruchamia `make security-test`. Zawiera też własną implementację obliczania SHA-256 (`boot/crypto/sha256.c`), sprawdzaną w ramach tych testów. Program nie jest jeszcze włączony do mechanizmu uruchamiania VGK i nie zapewnia ochrony podczas startu.

Moduł sprawdzania integralności plików w języku C (`boot/crypto/integrity.c`) umożliwia porównanie skrótu SHA-256 danych w pamięci lub pliku odczytywanego porcjami. Testy uruchamiają `make integrity-test` oraz `make security-test`. Moduł nie jest jeszcze podłączony do programu uruchamiania i sam nie potwierdza autora pliku — wymaga zaufanego skrótu z podpisanego wykazu.

W katalogu `boot/crypto/` znajduje się również interfejs weryfikacji podpisów Ed25519. Wykorzystuje on zachowaną bibliotekę Monocypher 4.0.3 (licencja BSD-2-Clause lub CC0-1.0); pochodzenie i warunki licencji opisano w `boot/crypto/monocypher/POCHODZENIE.md`. Test wektora RFC 8032 uruchamia `make ed25519-test`. Weryfikator nie jest jeszcze podłączony do programu UEFI ani nie blokuje startu.

Dodano także parser podpisanego wykazu plików VGK (format VGKM) w języku C. Sprawdza podpis przed odczytaniem wpisów i odrzuca niebezpieczne lub powtórzone ścieżki. Test: `make manifest-test`. Moduł nie jest jeszcze połączony z programem UEFI.

Podpisany wykaz VGK można już przekazać do wspólnej kontroli plików: funkcja `vgk_manifest_verify_files` sprawdza skróty plików wskazanych w podpisanym wykazie, korzystając z funkcji odczytu dostarczonej przez przyszłą obsługę systemu plików. Test obejmuje również wykrycie podmienionej zawartości. Integracja z rzeczywistym odczytem nośnika UEFI jest kolejnym etapem.


## Planowane aplikacje użytkowe

W wymaganiach VGK zapisano własny kreator przygotowania pendrive'a instalacyjnego (wygodny w obsłudze jak Rufus), osobny instalator systemu oraz pakiet biurowy obejmujący edytor tekstu, arkusz kalkulacyjny i prezentacje. Są to cele rozwoju, a nie funkcje dostępne w obecnym prototypie.


## Walidacja jądra przed załadowaniem

Dodano eksperymentalny walidator ELF64 x86-64, który sprawdza strukturę i bezpieczne granice segmentów przed przygotowaniem planu załadowania. Nie ładuje jeszcze jądra do pamięci ani nie przekazuje mu sterowania. Test walidatora jest częścią kontroli GitHub Actions; wynik należy potwierdzić po zakończeniu przebiegu.
