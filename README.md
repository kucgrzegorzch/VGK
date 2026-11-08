# VGK – niezależny system operacyjny x86-64 (prototyp)

VGK jest eksperymentalnym projektem własnego systemu operacyjnego dla komputerów x86-64. Jądro i docelowy mechanizm uruchamiania są rozwijane jako niezależny kod projektu, bez używania jądra Linuksa lub Windows. Projekt nie jest jeszcze kompletnym systemem do codziennego użytku.

## Aktualny stan – listopad 2026

**Własna ścieżka UEFI uruchomiła już natywne jądro VGK w emulatorze QEMU z firmware OVMF.** Test GitHub Actions nr 349 zakończył się powodzeniem. Dziennik portu szeregowego potwierdził:
- sprawdzenie podpisu i plików wydania;
- załadowanie i uruchomienie własnego jądra;
- poprawną walidację protokołu `VGK_BOOT_INFO`.

To potwierdza start w konkretnym środowisku emulatora. Nie jest jeszcze potwierdzeniem działania na fizycznym komputerze.

## Co jest już zaimplementowane i sprawdzone

- Własna aplikacja startowa UEFI x86-64.
- Odczyt plików z woluminu uruchomieniowego przez protokoły UEFI.
- Podpisany wykaz VGKM i weryfikacja Ed25519.
- Kontrola rozmiaru i skrótu SHA-256 plików, w tym ponowna kontrola jądra bezpośrednio przed załadowaniem.
- Ograniczony walidator ELF64 x86-64: granice segmentów, adresy, wyrównanie, nakładanie stron, punkt wejścia i zakaz segmentu W+X.
- Przydzielanie dokładnych zakresów pamięci przez UEFI, kopiowanie segmentów i zerowanie BSS.
- Normalizacja mapy pamięci UEFI do wspólnego formatu VGK.
- Wersjonowany protokół przekazania danych startowych do jądra.
- Zakończenie usług startowych UEFI i przekazanie sterowania do własnego punktu wejścia jądra.
- Własny stos jądra oraz diagnostyka przez port szeregowy COM1.
- Test uruchomienia w QEMU/OVMF z tymczasowym kluczem i podpisanym wydaniem testowym.

## Ograniczenia i dalsze prace

- Test QEMU używa tymczasowego klucza utworzonego wyłącznie w GitHub Actions. Nie jest to klucz produkcyjny VGK.
- Oficjalny klucz publiczny wydawcy nie został jeszcze ustalony i zatwierdzony. Zwykły obraz `BOOTX64.EFI` bez klucza zaufania działa w trybie odmowy startu – nie uruchamia niezweryfikowanego jądra.
- Nie wykonano jeszcze testu na fizycznym komputerze.
- Własny wariant uruchamiania starszego BIOS/Legacy nie jest gotowy.
- Jądro jest na razie minimalnym etapem diagnostycznym. Nie ma jeszcze pełnego zarządzania pamięcią i procesami, własnych tablic stron, obsługi przerwań, sterowników, systemu plików, sieci ani pulpitu.
- Instalator, aplikacje, kopie zapasowe, środowisko ratunkowe i Inteligentny Opiekun Systemu są planowanymi elementami, a nie gotowymi funkcjami.

## Budowanie

Wymagane są GCC, Clang, GNU binutils, GNU Make i Python 3.

```sh
make clean uefi
make native-kernel
make native-kernel-test
```

Testy bezpieczeństwa i modułów można uruchomić przez:

```sh
make security-test
```

GitHub Actions dodatkowo buduje osobny obraz UEFI z kluczem tymczasowym, przygotowuje podpisane wydanie testowe i próbuje uruchomić jądro w QEMU/OVMF. Klucz prywatny i obraz testowy nie są publikowane jako oficjalne wydanie.

## Historyczne ISO VGK 0.1

Polecenie `make iso` nadal tworzy historyczny prototyp BIOS/Multiboot2 korzystający z GRUB. Nie jest to nowa, natywna ścieżka UEFI i nie należy traktować tego ISO jako nośnika instalacyjnego aktualnego VGK.

## Wymagania docelowego systemu

Wymagania dotyczące bezpieczeństwa, instalatora, aplikacji, kopii zapasowych, pulpitu i trybu awaryjnego opisano w:
- [Wymagania użytkowe VGK](docs/WYMAGANIA_UZYTKOWE_VGK.md)
- [Plan wdrożenia](docs/PLAN_WDROZENIA.md)
- [Projekt trybu awaryjnego](docs/TRYB_AWARYJNY.md)
- [Mechanizm bezpieczeństwa i zaufania](docs/BEZPIECZENSTWO.md)
- [Protokół startowy VGK](docs/PROTOKOL_STARTOWY.md)

VGK pozostaje projektem w fazie rozwoju. Kolejne etapy będą oznaczane osobno jako zaimplementowane, sprawdzone w emulatorze i sprawdzone na sprzęcie.
