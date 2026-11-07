# Protokol startowy VGK – wersja 1

## Cel

Protokol jest wspolnym kontraktem pomiedzy wlasnym mechanizmem uruchamiania VGK a jadrem. Ma byc taki sam niezaleznie od tego, czy komputer uruchomiono przez UEFI, czy przez starszy BIOS. Jadro nie powinno odczytywac surowych struktur firmware.

Definicje znajduja sie w `include/vgk_boot_info.h`.

## Zasady zgodnosci

- Sygnatura: `VGK_BOOT_INFO_MAGIC`.
- Wersja: `VGK_BOOT_INFO_VERSION = 1`.
- Wszystkie adresy i rozmiary w strukturze maja stale pola 64-bitowe.
- Rozmiar struktury jest przekazywany w polu `structure_size`.
- Nieznane flagi oraz niezerowe pola zarezerwowane powoduja odrzucenie danych.
- Wpis mapy pamieci ma staly rozmiar 32 bajtow i opisuje zakres, dlugosc, rodzaj oraz atrybuty.
- Mapa pamieci jest znormalizowana przez ladowacz. Jadro nie interpretuje bezposrednio deskryptorow UEFI ani struktur BIOS.

## Dane przekazywane do jadra

1. Typ firmware: UEFI albo BIOS.
2. Adres, rozmiar i format znormalizowanej mapy pamieci.
3. Fizyczny adres i rozmiar zaladowanego obrazu jadra.
4. Adres wejscia jadra.
5. Opcjonalny opis bufora obrazu (adres, rozmiar, wymiary, krok wiersza i format).
6. Opcjonalny adres tabel RSDP ACPI.

## Walidacja

Funkcja `vgk_boot_info_is_valid` sprawdza:
- znacznik, wersje i minimalny rozmiar struktury;
- rozpoznany typ firmware i dozwolone flagi;
- wyrownanie adresu mapy oraz calkowita liczbe wpisow;
- zgodnosc rozmiaru wpisu mapy;
- obecność podstawowych informacji o zaladowanym jadrze;
- spojność opcjonalnych danych obrazu i ACPI;
- wyzerowanie pol zarezerwowanych.

Walidacja struktury nie dowodzi, ze wskazane adresy sa dostepne lub bezpieczne. Ladowacz musi zapewnic poprawne mapowanie, zakresy pamieci i czas zycia danych. Jadro po przejeciu sterowania musi dodatkowo sprawdzic zakresy przed dereferencja.

## Przekazanie sterowania – wymagany przyszly przebieg

1. Ladowacz odczytuje wydanie i weryfikuje podpis oraz skroty plikow.
2. Ladowacz laduje jadro do pamieci i przygotowuje znormalizowana mape.
3. Ladowacz wypelnia `VGK_BOOT_INFO` i sprawdza ja funkcja walidujaca.
4. Ladowacz konczy uslugi startowe firmware w odpowiednim momencie.
5. Ladowacz przekazuje adres struktury do uzgodnionego punktu wejscia jadra zgodnie z ustalonym ABI x86-64.
6. Ladowacz przekazuje wskaznik do struktury w rejestrze RDI; jest to wlasny, prosty kontrakt VGK dla x86-64, a nie ABI GRUB/Multiboot2.\n7. Jadro sprawdza protokol przed korzystaniem z przekazanych danych.

## Status implementacji

- [x] Definicja wersjonowanej struktury.
- [x] Definicja wspolnego wpisu mapy pamieci.
- [x] Walidacja podstawowej spojnosci.
- [x] Testy przypadkow poprawnych i blednych.
- [x] Test uruchamiany w GitHub Actions\n- [x] Minimalny punkt wejscia jadra i kontrola protokolu przez port szeregowy (kod przygotowany; bez uruchomienia przez ladowacz).
- [ ] Wypelnianie struktury przez ladowacz UEFI.
- [ ] Zaladowanie obrazu jadra przez UEFI.
- [ ] Przekazanie sterowania do jadra.
- [ ] Implementacja ladowacza BIOS.
- [ ] Test uruchomienia w emulatorze UEFI i na sprzecie.

Testy kompilacji i walidacji struktury nie oznaczaja jeszcze, ze VGK uruchamia jadro. Ten etap pozostaje do wykonania.
