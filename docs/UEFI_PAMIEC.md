# Pamiec i uslugi startowe UEFI w VGK

## Cel

Wlasny ladowacz VGK potrzebuje kontrolowanego dostepu do pamieci firmware, mapy pamieci oraz mozliwosci zakonczenia uslug startowych UEFI. Deklaracje ABI znajduja sie w `boot/uefi/uefi_services.h`.

## Zaimplementowane deklaracje ABI

- `AllocatePages` – rezerwowanie stron pamieci na obraz jadra i dane startowe.
- `FreePages` – zwalnianie stron zarezerwowanych podczas nieudanej proby.
- `GetMemoryMap` – pobieranie mapy pamieci i klucza mapy.
- `ExitBootServices` – zakonczenie uslug startowych przed przekazaniem sterowania jadru.
- `VGK_EFI_MEMORY_DESCRIPTOR` – znany prefiks deskryptora UEFI.

Uklady struktur sa sprawdzane statycznie i testowane przez `tests/test_uefi_services_abi.c`. Samo zadeklarowanie funkcji nie oznacza jeszcze, ze ladowacz je wywoluje.

## Normalizacja mapy pamieci

`vgk_uefi_normalize_memory_map` przeksztalca tablice deskryptorow UEFI w staly format VGK:
- obsluguje wiekszy krok deskryptora niz znany prefiks 40 bajtow;
- sprawdza wersje, rozmiar, wyrownanie, liczbe stron i przeplenienia adresow;
- mapuje rodzaje pamieci firmware na typy VGK;
- sortuje zakresy wedlug adresu fizycznego;
- odrzuca nakladajace sie zakresy;
- nie alokuje pamieci i nie wywoluje firmware – bufory zapewnia ladowacz.

Typy pamieci uslug startowych i ladowacza sa oznaczane jako obszary ladowacza. Po poprawnym zakonczeniu uslug startowych ladowacz moze przekazac je do puli pamieci odzyskiwalnej, ale dopiero gdy nie sa juz uzywane przez kod ani dane VGK.

## Wymagana kolejnosc pracy przyszlego ladowacza

1. Odczytac i zweryfikowac podpisany wykaz wydania oraz pliki.
2. Odczytac naglowek ELF jadra i sprawdzic wszystkie granice segmentow.
3. Zarezerwowac strony przez `AllocatePages` i wczytac segmenty PT_LOAD do ich adresow fizycznych.
4. Przygotowac stos, strukture `VGK_BOOT_INFO` i bufor znormalizowanej mapy.
5. Pobrac aktualna mape przez `GetMemoryMap`; po kazdej alokacji lub zwolnieniu pobrac ja ponownie.
6. Wywolac `ExitBootServices` z najnowszym kluczem mapy. Jesli firmware zwroci blad z powodu zmiany mapy, ponownie pobrac mape i ponowic probe w ograniczonej petli.
7. Po sukcesie nie wywolywac juz zadnych uslug startowych UEFI.
8. Przekazac sterowanie do `vgk_kernel_entry` z adresem `VGK_BOOT_INFO` w RDI.

## Status

- [x] Deklaracje i kontrola ukladu podstawowych funkcji UEFI.
- [x] Deklaracja deskryptora mapy pamieci UEFI.
- [x] Funkcja normalizacji mapy do formatu VGK.
- [x] Testy sortowania, typow, rozszerzonego kroku deskryptora i blednych zakresow.
- [ ] Wywolanie AllocatePages przez ladowacz.
- [ ] Odczyt i zaladowanie segmentow ELF.
- [ ] Pobranie mapy i obsluga ponowienia ExitBootServices.
- [ ] Rzeczywiste przekazanie sterowania do jadra.
- [ ] Test w emulatorze UEFI.

## Ograniczenia bezpieczenstwa

Mapa firmware jest danymi wejsciowymi. Ladowacz musi sprawdzac kazdy rozmiar i adres przed kopiowaniem. Nie wolno zwalniac pamieci, w ktorej nadal znajduje sie kod ladowacza, stos, mapa lub struktura startowa. Po ExitBootServices zadne wywolanie uslug startowych nie jest dozwolone.
