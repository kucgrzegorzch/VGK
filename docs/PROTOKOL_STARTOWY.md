# Protokół startowy VGK – wersja 2

## Cel

Protokół jest wspólnym kontraktem pomiędzy własnym mechanizmem uruchamiania VGK a jądrem. Ma być niezależny od surowych struktur firmware, tak aby w przyszłości mógł go wypełniać zarówno ładowacz UEFI, jak i osobny ładowacz starszego BIOS.

Definicje znajdują się w `include/vgk_boot_info.h`.

## Zasady zgodności

- Sygnatura: `VGK_BOOT_INFO_MAGIC`.
- Wersja: `VGK_BOOT_INFO_VERSION = 2`.
- Adresy i rozmiary mają stałe pola 64-bitowe.
- Rozmiar struktury jest przekazywany w polu `structure_size`.
- Nieznane flagi oraz niezerowe pola zarezerwowane powodują odrzucenie danych.
- Wpis mapy pamięci ma stały rozmiar 32 bajtów i opisuje zakres, długość, rodzaj oraz atrybuty.
- Mapa pamięci jest normalizowana przez ładowacz. Jądro nie interpretuje bezpośrednio deskryptorów UEFI ani struktur BIOS.

## Dane przekazywane do jądra

1. Typ firmware: UEFI albo BIOS.
2. Adres, rozmiar i format znormalizowanej mapy pamięci.
3. Fizyczny adres i rozmiar załadowanego obrazu jądra.
4. Adres wejścia jądra.
5. Opcjonalny opis bufora obrazu: adres, rozmiar, wymiary, krok wiersza i format.
6. Opcjonalny adres tabel RSDP ACPI.

W obecnej ścieżce UEFI wypełniane są dane dotyczące firmware, mapy pamięci i jądra. Informacje o framebufferze i ACPI pozostają nieaktywne – odpowiednie flagi są wyzerowane.

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
- spójność opcjonalnych danych obrazu i ACPI;
- wyzerowanie pól zarezerwowanych;
- przepełnienia zakresów adresowych.

Walidacja struktury nie dowodzi sama w sobie, że wskazane adresy są dostępne lub bezpieczne. Ładowacz musi zapewnić poprawne mapowanie, zakresy pamięci i czas życia danych. Jądro musi dodatkowo sprawdzać zakresy przed dereferencją.

## Przekazanie sterowania – obecna ścieżka UEFI

1. Ładowacz otwiera wolumin startowy i odczytuje podpisany wykaz VGKM oraz podpis.
2. Sprawdza podpis Ed25519 i integralność plików wymienionych w wykazie.
3. Odczytuje plik jądra do ograniczonego bufora i ponownie sprawdza jego rozmiar oraz SHA-256.
4. Waliduje ELF64 x86-64, planuje segmenty i przydziela dokładne zakresy pamięci przez `AllocatePages`.
5. Kopiuje segmenty do pamięci i zeruje obszary BSS.
6. Normalizuje mapę pamięci UEFI i wypełnia `VGK_BOOT_INFO`.
7. Pobiera aktualną mapę bezpośrednio przed `ExitBootServices`. Przy `EFI_INVALID_PARAMETER` ponawia pobranie mapy i próbę w ograniczonej pętli. Po nieudanej pierwszej próbie nie wykonuje innych usług firmware; przy nieoczekiwanym błędzie zatrzymuje procesor.
8. Po udanym zakończeniu usług UEFI most asemblerowy przekazuje wskaźnik `VGK_BOOT_INFO` w rejestrze RDI i skacze do punktu wejścia jądra. Jest to własny kontrakt VGK, a nie ABI GRUB/Multiboot2.
9. Punkt wejścia jądra ustawia własny stos, wywołuje funkcję diagnostyczną i sprawdza protokół przed użyciem danych.

## Status implementacji

- [x] Definicja wersjonowanej struktury.
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
- [x] Test uruchomienia w QEMU z firmware OVMF i tymczasowym kluczem testowym (GitHub Actions, przebieg nr 349).
- [ ] Potwierdzenie działania na fizycznym komputerze.
- [ ] Produkcyjny klucz zaufania wydawcy i procedura jego rotacji.
- [ ] Implementacja ładowacza starszego BIOS.
- [ ] Rozszerzenie protokołu o framebuffer, ACPI i dalsze informacje sprzętowe.

## Granice obecnego testu

Test QEMU potwierdza, że określony obraz UEFI, podpisane wydanie testowe i diagnostyczne jądro uruchomiły się w określonej konfiguracji emulatora OVMF. Klucz jest tworzony tymczasowo w GitHub Actions i nie jest kluczem produkcyjnym. Protokół v2 przekazuje uprawnienia segmentów do przyszłego zarządcy tablic stron; samo przekazanie flag nie oznacza jeszcze, że procesor egzekwuje te uprawnienia.

Nie potwierdza to jeszcze zgodności ze wszystkimi wersjami firmware, procesorami Intel i AMD ani uruchomienia na fizycznym komputerze. Jądro pozostaje minimalnym etapem diagnostycznym, bez pełnego zarządzania pamięcią, przerwań, sterowników i usług systemowych.
