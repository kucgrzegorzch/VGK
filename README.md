# VGK – niezależny system operacyjny x86-64 (prototyp)

VGK jest eksperymentalnym projektem własnego systemu operacyjnego dla komputerów x86-64. Jądro i docelowy mechanizm uruchamiania są rozwijane jako niezależny kod projektu, bez używania jądra Linuksa lub Windows. Projekt nie jest jeszcze kompletnym systemem do codziennego użytku.

## Aktualny stan – 12 listopada 2026

**Natywna ścieżka UEFI x86-64 uruchamia własne jądro VGK w emulatorze QEMU z firmware OVMF.** Aktualna gałąź rozwojowa używa wersji 4 protokołu startowego. Protokół przekazuje między innymi znormalizowaną mapę pamięci, segmenty ELF, inwentarz PCI oraz zarezerwowaną przez UEFI pulę 64 stron po 4 KiB na przygotowanie nieaktywnych tablic stron.

Najnowszy przebieg GitHub Actions powiązany z PR #13 (run 836) zakończył się sukcesem. Wszystkie 40 kroków zadania zakończyło się powodzeniem. Testy obejmują kompilację aplikacji UEFI i jądra, walidację ELF64, alokator pamięci, ACPI, wykrywanie CPU, przerwania i wyjątki, framebuffer, PCI, format podpisanego wydania oraz uruchomienie jądra w QEMU/OVMF. Osobny test potwierdził odmowę uruchomienia jądra po zmianie jego zawartości.

To potwierdza działanie w określonym środowisku emulatora. Nie jest potwierdzeniem uruchomienia na fizycznym komputerze ani zgodności ze wszystkimi komputerami x86-64.

## Co jest już zaimplementowane i sprawdzone

- Własna aplikacja startowa UEFI x86-64.
- Odczyt plików z woluminu uruchomieniowego przez protokoły UEFI.
- Podpisany wykaz VGKM i weryfikacja Ed25519.
- Kontrola rozmiaru i skrótu SHA-256 plików, w tym ponowna kontrola jądra bezpośrednio przed załadowaniem.
- Ograniczony walidator ELF64 x86-64: granice segmentów, adresy, wyrównanie, nakładanie stron, punkt wejścia i zakaz segmentu W+X.
- Przydzielanie dokładnych zakresów pamięci przez UEFI, kopiowanie segmentów i zerowanie BSS.
- Normalizacja mapy pamięci UEFI do wspólnego formatu VGK.
- Wczesny zarządca fizycznych stron pamięci: wyznacza wolne zakresy, przydziela strony z wyrównaniem i scala zwalniane zakresy.
- Walidacja segmentów jądra w protokole startowym: odrzucanie zakresów nakładających się, nieprawidłowego punktu wejścia i segmentów jednocześnie zapisywalnych oraz wykonywalnych.
- Wersjonowany protokół przekazania danych startowych do jądra (wersja 4), z opisami segmentów ELF, ich uprawnieniami, maksymalnie 128 znormalizowanymi identyfikatorami PCI i pulą pamięci zarezerwowaną na tablice stron.
- Skaner UEFI PCI I/O odczytujący podstawowe identyfikatory urządzeń przed ExitBootServices; pełny przepływ do jądra został sprawdzony w CI/QEMU.
- Własny stos jądra oraz diagnostyka przez port szeregowy COM1.
- Wykrywanie wybranych możliwości procesora przez CPUID, w tym obsługi NX i szerokości adresu fizycznego.
- Przygotowanie nieaktywnych tablic stron x86-64 z kontrolą podstawowych uprawnień, NX, W^X i mapowania framebufferu. Tablice nie są jeszcze aktywowane przez przełączenie CR3.
- Test uruchomienia w QEMU/OVMF z tymczasowym kluczem i podpisanym wydaniem testowym.
- Test odmowy startu po celowej modyfikacji pliku jądra.

## Ograniczenia i dalsze prace

- Test QEMU używa tymczasowego klucza utworzonego wyłącznie w GitHub Actions. Nie jest to klucz produkcyjny VGK.
- Oficjalny klucz publiczny wydawcy nie został jeszcze ustalony i zatwierdzony. Zwykły obraz `BOOTX64.EFI` bez klucza zaufania działa w trybie odmowy startu – nie uruchamia niezweryfikowanego jądra.
- Nie wykonano jeszcze testu na fizycznym komputerze.
- Wykrywanie PCI nie oznacza jeszcze obsługi sterowników; odczyt BAR/MMIO, DMA i sterowniki urządzeń pozostają odrębnymi etapami.
- Własny wariant uruchamiania starszego BIOS/Legacy nie jest gotowy.
- Tablice stron są na razie budowane i sprawdzane jako nieaktywne. Nie przełączamy CR3 ani nie włączamy globalnie CR0.WP lub EFER.NXE.
- Jądro jest na razie minimalnym etapem diagnostycznym. Nie ma jeszcze pełnego zarządzania procesami, aktywnej ochrony pamięci przez własne tablice stron, kompletu obsługi przerwań, sterowników, systemu plików, sieci ani pulpitu.
- Instalator, aplikacje, kopie zapasowe, środowisko ratunkowe i Inteligentny Opiekun Systemu są planowanymi elementami, a nie gotowymi funkcjami.
- Plan obsługi grafiki, kart graficznych, sterowników i aplikacji multimedialnych jest dokumentacją projektową, a nie gotową implementacją.

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

GitHub Actions dodatkowo buduje osobny obraz UEFI z kluczem tymczasowym, przygotowuje podpisane wydanie testowe i uruchamia jądro w QEMU/OVMF. Klucz prywatny i obraz testowy nie są publikowane jako oficjalne wydanie.

## Pakiet testowy UEFI USB

Workflow CI przygotowuje eksperymentalny pakiet plików do testowego nośnika UEFI/FAT32. Pakiet zawiera aplikację startową, podpisany manifest, jądro i instrukcję. Nie jest to obraz ISO, instalator ani wydanie produkcyjne. Używa tymczasowego klucza CI.

## Historyczne ISO VGK 0.1

Polecenie `make iso` nadal tworzy historyczny prototyp BIOS/Multiboot2 korzystający z GRUB. Nie jest to nowa, natywna ścieżka UEFI i nie należy traktować tego ISO jako nośnika instalacyjnego aktualnego VGK.

## Wymagania docelowego systemu

Wymagania dotyczące bezpieczeństwa, instalatora, aplikacji, kopii zapasowych, pulpitu i trybu awaryjnego opisano w:
- [Wymagania użytkowe VGK](docs/WYMAGANIA_UZYTKOWE_VGK.md)
- [Plan wdrożenia](docs/PLAN_WDROZENIA.md)
- [Projekt trybu awaryjnego](docs/TRYB_AWARYJNY.md)
- [Mechanizm bezpieczeństwa i zaufania](docs/BEZPIECZENSTWO.md)
- [Protokół startowy VGK](docs/PROTOKOL_STARTOWY.md)
- [Plan grafiki, sterowników i multimediów](docs/PLAN_GRAFIKI_I_MULTIMEDIOW.md)
- [Plan wykrywania urządzeń PCI/PCIe](docs/PLAN_WYKRYWANIA_PCI.md)

VGK pozostaje projektem w fazie rozwoju. Kolejne etapy będą oznaczane osobno jako zaprojektowane, zaimplementowane, sprawdzone statycznie, sprawdzone w emulatorze i sprawdzone na sprzęcie.
