# Pamiec i uslugi startowe UEFI w VGK

## Cel

Wlasny ladowacz VGK potrzebuje kontrolowanego dostepu do pamieci firmware, mapy pamieci oraz mozliwosci zakonczenia uslug startowych UEFI. Deklaracje ABI znajduja sie w `boot/uefi/uefi_services.h`.

## Zaimplementowane ABI i obsluga pamieci

- `AllocatePages` i `FreePages` – rezerwowanie oraz zwalnianie stron podczas pracy ladowacza.
- `GetMemoryMap` – pobieranie mapy pamieci i klucza mapy.
- `ExitBootServices` – zakonczenie uslug startowych przed przekazaniem sterowania jadru.
- `VGK_EFI_MEMORY_DESCRIPTOR` – obsluga znanego prefiksu deskryptora UEFI oraz rozszerzonego kroku deskryptora.
- `vgk_uefi_normalize_memory_map` – normalizacja typow i zakresow do formatu VGK.
- `boot/uefi/uefi_memory_map.c` – przygotowanie mapy przekazywanej jadru.
- `boot/uefi/elf_loader.c` i `uefi_elf_allocator.c` – walidacja ELF64, rezerwacja zakresow segmentow PT_LOAD, kopiowanie danych i zerowanie BSS.
- `VGK_BOOT_INFO` – przekazanie adresu i rozmiaru mapy, segmentow jadra oraz osobnej puli stron na przyszle tablice stron.

Uklady struktur i funkcji sa sprawdzane statycznie i testowane przez cele Makefile. Samo zadeklarowanie ABI nie jest dowodem poprawnego dzialania; aktualna sciezka zostala dodatkowo uruchomiona w QEMU/OVMF.

## Normalizacja mapy pamieci

Normalizator:
- obsluguje wiekszy krok deskryptora niz znany prefiks;
- sprawdza wersje, rozmiar, wyrownanie, liczbe stron i przepelnienia adresow;
- mapuje rodzaje pamieci firmware na typy VGK;
- sortuje zakresy wedlug adresu fizycznego;
- odrzuca nakladajace sie lub niepoprawne zakresy;
- nie alokuje pamieci i nie wywoluje firmware – bufory zapewnia ladowacz.

Typy pamieci uslug startowych i ladowacza sa oznaczane jako obszary ladowacza. Nie wolno odzyskiwac stron, w ktorych nadal znajduja sie kod, stos lub dane potrzebne jadru.

## Aktualna kolejnosc pracy ladowacza

1. Odczytuje i weryfikuje podpisany wykaz VGKM oraz pliki wydania.
2. Waliduje naglowek ELF64 i granice segmentow.
3. Rezerwuje strony przez UEFI, laduje segmenty PT_LOAD, kopiuje dane i zeruje BSS.
4. Przygotowuje stos, strukture `VGK_BOOT_INFO`, mape pamieci i pule stron.
5. Pobiera aktualna mape przez `GetMemoryMap`; po zmianie alokacji musi uwzglednic nowy klucz mapy.
6. Wywoluje `ExitBootServices` z aktualnym kluczem mapy.
7. Po sukcesie nie korzysta juz z uslug startowych UEFI.
8. Przekazuje sterowanie do `vgk_kernel_entry` z adresem `VGK_BOOT_INFO` w RDI.

## Pula tablic stron

Ladowacz rezerwuje osobna pule stron typu `EfiLoaderData`, zeruje ja i przekazuje jej adres oraz liczbe stron w `VGK_BOOT_INFO`. Jadro sprawdza, czy caly zakres nalezy do pamieci oznaczonej jako obszar ladowacza, czy nie koliduje ze struktura startowa, mapa pamieci, segmentami jadra ani framebufferem oraz czy strony sa wyzerowane.

Jadro buduje w tej puli struktury czteropoziomowych tablic stron, ale na obecnym etapie nie aktywuje ich przez zapis do CR3. Testy sprawdzaja budowanie i walidacje struktur bez przelaczania aktywnej przestrzeni adresowej. Nie wlaczamy przedwczesnie CR0.WP ani EFER.NXE.

## Status weryfikacji

- [x] Deklaracje i kontrola ukladu podstawowych funkcji UEFI.
- [x] Normalizacja mapy pamieci UEFI i testy blednych zakresow.
- [x] Rezerwacja pamieci jadra i danych startowych.
- [x] Walidacja ELF64, przydzial segmentow PT_LOAD, kopiowanie i zerowanie BSS.
- [x] Przygotowanie puli tablic stron przez ladowacz i przekazanie jej w protokole startowym.
- [x] Przekazanie sterowania do wlasnego jadra w QEMU/OVMF.
- [x] Test dostepnosci, wyzerowania i rezerwacji puli stron w jadru.
- [x] Testy regresyjne CI w przebiegach 830 i 831 (11.11.2026).
- [ ] Test uruchomienia na fizycznym komputerze UEFI.

## Ograniczenia bezpieczenstwa

Mapa firmware jest danymi wejsciowymi. Kazdy rozmiar i adres musi byc sprawdzony przed kopiowaniem lub mapowaniem. Nie wolno zwalniac pamieci, w ktorej nadal znajduje sie kod ladowacza, stos, mapa lub struktura startowa. Po `ExitBootServices` zadne wywolanie uslug startowych nie jest dozwolone.

Test QEMU/OVMF nie dowodzi zgodnosci ze wszystkimi implementacjami UEFI ani z fizycznymi komputerami. Starsza sciezka BIOS/Legacy nie jest jeszcze gotowa i musi w przyszlosci zapewnic rownowazny kontrakt startowy.
