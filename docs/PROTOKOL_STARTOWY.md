# Protokół startowy VGK – wersja 4

## Cel

Protokół jest wspólnym kontraktem pomiędzy własnym mechanizmem uruchamiania VGK a jądrem. Ma być niezależny od surowych struktur firmware, tak aby w przyszłości mógł go wypełniać zarówno ładowacz UEFI, jak i osobny ładowacz starszego BIOS.

Definicje znajdują się w `include/vgk_boot_info.h`.

## Zasady zgodności

- Sygnatura: `VGK_BOOT_INFO_MAGIC`.
- Wersja: `VGK_BOOT_INFO_VERSION = 4`.
- Adresy i rozmiary mają stałe pola 64-bitowe.
- Rozmiar struktury jest przekazywany w polu `structure_size`.
- Nieznane flagi oraz niezerowe pola zarezerwowane powodują odrzucenie danych.
- Wpis mapy pamięci ma stały rozmiar 32 bajtów i opisuje zakres, długość, rodzaj oraz atrybuty.
- Mapa pamięci jest normalizowana przez ładowacz. Jądro nie interpretuje bezpośrednio deskryptorów UEFI ani struktur BIOS.

## Dane przekazywane do jądra

1. Typ firmware: UEFI albo BIOS.
2. Adres, rozmiar i format znormalizowanej mapy pamięci.
3. Fizyczny adres i rozmiar załadowanego obrazu jądra.
4. Fizyczny adres i liczbę stron zarezerwowanej puli tablic stron (64 strony po 4 KiB). Pula jest przydzielana jako pamięć ładowacza przez UEFI, zerowana przed zakończeniem usług firmware i pozostaje zarezerwowana dla buildera tablic stron.
4. Adres wejścia jądra.
5. Opcjonalny opis bufora obrazu: adres, rozmiar, wymiary, krok wiersza i format.
6. Opcjonalny adres tabel RSDP ACPI.
7. Ograniczona lista znormalizowanych identyfikatorów urządzeń PCI (maksymalnie 128), bez uchwytów i wskaźników firmware; flaga informuje o ewentualnym obcięciu listy.

W obecnej ścieżce UEFI wypełniane są dane dotyczące firmware, mapy pamięci i jądra. Ładowacz próbuje również pobrać bieżący tryb UEFI GOP z uchwytu konsoli tekstowej. Jeśli bufor jest liniowy, ma format RGB/BGR 8-bit na kanał i przechodzi kontrolę wymiarów, kroku wiersza oraz zakresu pamięci, jego opis trafia do protokołu. Nieobsługiwany format lub brak GOP pozostawia flagę framebuffer wyzerowaną. Informacje ACPI pozostają nieaktywne.

## Walidacja

Funkcja `vgk_boot_info_is_valid` sprawdza:
- znacznik, wersję i minimalny rozmiar struktury;
- rozpoznany typ firmware i dozwolone flagi;
- wyrównanie adresu mapy oraz całkowitą liczbę wpisów;
- zgodność rozmiaru wpisu mapy;
- podstawowe informacje o załadowanym jądrze i położenie punktu wejścia;
- liczbę i zakresy segmentów jądra, ich wyrównanie, dozwolone flagi oraz brak nakładania stron;
- zakaz segmentów jednocześnie zapisywalnych i wykonywalnych (W+X) oraz położenie punktu wejścia w segmencie wykonywalnym;
- wyzerowanie nieużywanych opisów segmentów;
- spójność opcjonalnych danych obrazu, ACPI i inwentarza PCI, w tym rozmiar rekordu, limity, unikalność adresu funkcji oraz wyzerowanie nieużywanych wpisów;
- wyzerowanie pól zarezerwowanych;
- przepełnienia zakresów adresowych;
- wyrównanie, dokładny rozmiar i brak kolizji puli tablic stron z jądrem oraz mapą pamięci. Dodatkowo inicjalizacja menedżera pamięci wymaga, aby cała pula była objęta wpisem lub ciągłym zestawem wpisów typu `VGK_MEMORY_BOOTLOADER`.

Walidacja struktury nie dowodzi sama w sobie, że wskazane adresy są dostępne lub bezpieczne. Ładowacz musi zapewnić poprawne mapowanie, zakresy pamięci i czas życia danych. Jądro musi dodatkowo sprawdzać zakresy przed dereferencją.

## Przekazanie sterowania – obecna ścieżka UEFI

1. Ładowacz otwiera wolumin startowy i odczytuje podpisany wykaz VGKM oraz podpis.
2. Sprawdza podpis Ed25519 i integralność plików wymienionych w wykazie.
3. Odczytuje plik jądra do ograniczonego bufora i ponownie sprawdza jego rozmiar oraz SHA-256.
4. Waliduje ELF64 x86-64, planuje segmenty i przydziela dokładne zakresy pamięci przez `AllocatePages`.
5. Kopiuje segmenty do pamięci i zeruje obszary BSS.
6. Przydziela przez `AllocatePages(EfiLoaderData)` 64 strony puli tablic stron i zeruje cały obszar, zanim pobierze końcową mapę pamięci.
6. Normalizuje mapę pamięci UEFI i wypełnia `VGK_BOOT_INFO`.
8. Pobiera aktualną mapę bezpośrednio przed `ExitBootServices`. Przy `EFI_INVALID_PARAMETER` ponawia pobranie mapy i próbę w ograniczonej pętli. Po nieudanej pierwszej próbie nie wykonuje innych usług firmware; przy nieoczekiwanym błędzie zatrzymuje procesor.
9. Po udanym zakończeniu usług UEFI most asemblerowy przekazuje wskaźnik `VGK_BOOT_INFO` w rejestrze RDI i skacze do punktu wejścia jądra. Jest to własny kontrakt VGK, a nie ABI GRUB/Multiboot2.
10. Punkt wejścia jądra ustawia własny stos, wywołuje funkcję diagnostyczną i sprawdza protokół przed użyciem danych.

## Status implementacji

- [x] Definicja wersjonowanej struktury.
- [x] Protokół v4: jawny adres i liczba stron 64-stronicowej puli tablic stron.
- [x] Rezerwacja i zerowanie puli przez UEFI przed końcowym pobraniem mapy pamięci.
- [x] Walidacja zakresu puli oraz wymaganie klasyfikacji jako pamięć ładowacza w inicjalizacji menedżera pamięci.
- [ ] Potwierdzenie pełnego przepływu v4 w CI/QEMU i testach regresyjnych.
- [x] Definicja wspólnego wpisu mapy pamięci.
- [x] Walidacja podstawowej spójności i przepełnień.
- [x] Testy przypadków poprawnych i błędnych.
- [x] Deklaracje usług UEFI i sprawdzenie ich położeń w tabeli firmware.
- [x] Normalizacja mapy UEFI do formatu VGK wraz z testami.
- [x] Odczyt wykazu i plików z woluminu UEFI.
- [x] Weryfikacja podpisu wykazu i skrótów plików.
- [x] Walidacja ELF64 oraz przydzielanie i kopiowanie segmentów.
- [x] Wypełnianie struktury przez ładowacz UEFI.
- [x] Przekazywanie zweryfikowanych uprawnień segmentów ELF w protokole wersji 2.
- [x] Testy odrzucania segmentów W+X, nakładających się stron i nieprawidłowego punktu wejścia.
- [ ] Wykorzystanie uprawnień segmentów do budowy tablic stron: tekst tylko do odczytu i wykonania, dane do zapisu i niewykonywalne.
- [x] Zakończenie usług startowych UEFI i przekazanie sterowania do jądra.
- [x] Minimalny punkt wejścia jądra, własny stos i diagnostyka przez port szeregowy.
- [x] Test uruchomienia w QEMU z firmware OVMF i tymczasowym kluczem testowym (GitHub Actions, przebieg nr 406).
- [ ] Potwierdzenie działania na fizycznym komputerze.
- [ ] Produkcyjny klucz zaufania wydawcy i procedura jego rotacji.
- [ ] Implementacja ładowacza starszego BIOS.
- [x] Pola protokołu dla framebufferu.
- [x] Pobieranie i walidacja trybu GOP RGB/BGR w ładowaczu UEFI; tryby bit-mask i BLT-only są na razie odrzucane.
- [ ] Przekazanie i wykorzystanie framebufferu przez jądro do rysowania obrazu.
- [x] Odczyt podstawowych identyfikatorów PCI przez protokół UEFI PCI I/O przed ExitBootServices.
- [x] Przekazanie ograniczonego inwentarza PCI w protokole startowym v3, bez wskaźników firmware.
- [ ] Potwierdzenie pełnego przepływu inwentarza PCI testem QEMU/OVMF i testami CI.
- [ ] Odczyt dalszych informacji PCIe i sterowniki urządzeń.
- [ ] Odczyt informacji ACPI.

## Zgodność z Legacy BIOS

Wersja 4 wymaga przekazania puli tablic stron przez każdy ładowacz. Obecnie zrealizowano wyłącznie ścieżkę UEFI. Przyszły ładowacz Legacy BIOS musi przygotować równoważny, wyrównany i wyłączony z alokatora zakres pamięci oraz przekazać go w tych samych polach protokołu. Nie wolno traktować pól puli jako opcjonalnych ani aktywować CR3, dopóki ścieżka BIOS nie spełni tego kontraktu.

## Granice obecnego testu

Test QEMU potwierdza, że określony obraz UEFI, podpisane wydanie testowe i diagnostyczne jądro uruchomiły się w określonej konfiguracji emulatora OVMF. Klucz jest tworzony tymczasowo w GitHub Actions i nie jest kluczem produkcyjnym. Protokół v3 przekazuje uprawnienia segmentów do przyszłego zarządcy tablic stron; samo przekazanie flag nie oznacza jeszcze, że procesor egzekwuje te uprawnienia.

Nie potwierdza to jeszcze zgodności ze wszystkimi wersjami firmware, procesorami Intel i AMD ani uruchomienia na fizycznym komputerze. Jądro pozostaje minimalnym etapem diagnostycznym, bez pełnego zarządzania pamięcią, przerwań, sterowników i usług systemowych.
