# Granica zapotrzebowania na tablice stron VGK

## Cel

Ten dokument wyznacza konserwatywną granicę liczby stron struktur czteropoziomowych tablic x86-64, które obecny protokół startowy może wymagać przy mapowaniu obiektów startowych. Jest podstawą do oceny przyszłego limitu puli. Nie implementuje jeszcze dynamicznej alokacji puli.

## Ograniczenia wejściowe potwierdzone w kodzie

- Segmenty ELF: maksymalnie 16; adresy fizyczne od 2 MiB do 1 GiB, bez współdzielenia stron. Łączny zakres adresowy jądra nie przekracza przedziału 2 MiB–1 GiB.
- Plik jądra: maksymalnie 64 MiB; rozmiar `p_memsz` segmentu może być większy od `p_filesz`, więc limit pliku nie jest limitem pamięci BSS.
- Znormalizowana mapa pamięci: maksymalnie 16 384 wpisy po 32 bajty, czyli 512 KiB.
- `VGK_BOOT_INFO`: 2376 bajtów przy obecnym układzie protokołu.
- ACPI RSDP: walidator dopuszcza standardową długość 20 lub 36 bajtów; do oszacowania przyjęto 36 bajtów.
- Pula tablic: protokół wersji 5 dopuszcza od 64 do 1024 stron po 4096 bajtów, czyli maksymalnie 4 MiB. Loader UEFI nadal przydziela początkowo 64 strony; dynamiczny wzrost nie jest jeszcze wdrożony.
- Framebuffer: maksymalny mapowany obszar obrazu ograniczony w tym etapie do 256 MiB. Walidator liczy `stride * height * 4`, a nie całą większą alokację GOP. Tryb przekraczający limit nie jest przekazywany jako aktywny framebuffer; jądro może kontynuować bez grafiki.

Limit 256 MiB obejmuje typowy 32-bitowy obraz 8K UHD 7680×4320 (około 126,6 MiB przy stride równym szerokości) i pozostawia miejsce na padding stride. Nie gwarantuje obsługi dowolnego trybu GOP.

## Konserwatywne liczenie

Kalkulator liczy unikalne prefiksy adresów dla czterech poziomów. Najgorszy przypadek nie polega wyłącznie na sumowaniu rozmiarów: małe zakresy mogą leżeć w oddzielnych gałęziach PML4 i przekraczać granice 2 MiB, 1 GiB oraz 512 GiB.

| Obiekt | Strony PT |
|---|---:|
| Jądro: cały zakres 2 MiB–1 GiB | 511 |
| Framebuffer: 256 MiB, niekorzystne wyrównanie i położenie | 129 |
| Pula: 4 MiB, przesunięcie o stronę względem granicy 2 MiB | 3 |
| Mapa pamięci: 512 KiB i niekorzystne położenie | 2 |
| `VGK_BOOT_INFO`: 2376 bajtów i niekorzystne położenie | 2 |
| ACPI RSDP: 36 bajtów i niekorzystne położenie | 2 |
| **Suma stron PT** | **649** |

Dodatkowo:
- PD: 11 stron;
- PDPT: 11 stron;
- korzeń PML4: 1 strona.

Łącznie: **649 + 11 + 11 + 1 = 672 strony struktur tablic stron**.

Test `test_worst_case_page_table_requirement()` w `tests/test_paging_requirements.c` odtwarza taki układ adresów: jądro w niskim obszarze, a pozostałe obiekty na oddzielnych granicach PML4. Test wymaga dokładnie 672 stron. Obejmuje również mapowanie samej puli 4 MiB, a więc nie pomija kosztu jej własnego mapowania.

## Kandydacki limit puli

1024 strony to 4 MiB pamięci zarezerwowanej na struktury tablic stron. Protokół i walidator jądra już egzekwują ten górny limit. Względem wyliczonego maksimum 672 stron pozostaje 352 strony zapasu.

Ten zapas nie jest obietnicą nieograniczonego rozwoju mapowań. Każdy nowy obiekt, większy zakres ELF, nowy poziom tablic albo zmiana protokołu wymaga ponownego przeliczenia granicy i aktualizacji testu. Jeśli wymagania przekroczą 672, należy ustalić, czy zmieniły się założenia; nie wolno po prostu zwiększać limitu bez audytu.

## Warunki i ograniczenia dowodu

1. Granica dotyczy obecnego zestawu mapowanych obiektów: segmentów ELF, `VGK_BOOT_INFO`, mapy pamięci, puli, ACPI RSDP i opcjonalnego framebufferu.
2. Zakłada czteropoziomowe stronicowanie i mapowanie liści 4 KiB, zgodnie z aktualnym kalkulatorem.
3. Zakresy fizyczne muszą być reprezentowalne jako kanoniczne adresy identity mapping. Test używa wysokich, ale nadal dolno-kanonicznych adresów fizycznych; sprzęt o mniejszej szerokości adresu fizycznego nie może wykorzystać części tych scenariuszy, więc ograniczenie jest konserwatywne.
4. Kalkulator strukturalny nie zastępuje kontroli fizycznych aliasów, uprawnień, szerokości adresów procesora ani poprawności danych firmware.
5. Wartość 672 jest granicą dla opisanych rozmiarów i liczby zakresów, nie dla przyszłych modułów VGK ani dodatkowych mapowań sterowników.
6. QEMU/OVMF i CI nie zastępują testu rozruchu na fizycznym komputerze.

## Status

- Limit mapowanego framebufferu 256 MiB i test graniczny znajdują się w Draft PR #26.
- Test najgorszego przypadku 672 stron znajduje się w kalkulatorze i przechodzi w CI PR #26.
- Protokół wersji 5 akceptuje zakres 64–1024 stron; jądro sprawdza wyzerowanie całego przekazanego obszaru. Loader UEFI nadal używa początkowych 64 stron.
- Dynamiczna alokacja UEFI, limit iteracji i obsługa braku pamięci pozostają osobnym, późniejszym etapem.
