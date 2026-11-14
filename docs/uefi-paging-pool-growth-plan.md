# Plan bezpiecznego wzrostu puli tablic stron UEFI

## Status

- Implementacja dynamicznego wzrostu znajduje się w Draft PR #28.
- Loader oblicza wymagania przed `ExitBootServices`, powiększa pulę do maksymalnie 1024 stron i po każdej alokacji/zwolnieniu ponownie pobiera mapę.
- Test polityki potwierdza sekwencję 64 → 129 → 258 → 516 → 1024.
- Test QEMU/OVMF kompiluje specjalny obraz wymuszający wzrost i potwierdza, że jądro otrzymuje pulę 1024 stron (`0x400`) oraz uruchamia się.
- CI #997 zakończyło się sukcesem. Nie jest to jeszcze test fizycznego komputera.
- Dedykowane testy awarii `AllocatePages`/`FreePages`, błędnych adresów oraz nieudanego retry `ExitBootServices` pozostają do dodania przed uznaniem wszystkich ścieżek błędów za w pełni pokryte.

## Ustalone limity

- Minimum: 64 strony.
- Rozmiar początkowy: 64 strony.
- Maksimum: 1024 strony (4 MiB).
- Udowodnione zapotrzebowanie obecnego zestawu mapowań: maksymalnie 672 strony.
- Rezerwa przy wzroście: 64 strony.
- Maksymalna liczba udanych powiększeń: 4.
- Maksymalna liczba prób `ExitBootServices`: nadal 4.

Polityka `vgk_paging_pool_next_size()` wybiera większą z wartości `required_pages + reserve_pages` i `current_pages * 2`, nie przekraczając 1024 stron. Każdy udany krok co najmniej podwaja pulę, więc od 64 do 1024 stron wystarczą najwyżej cztery powiększenia. Przy aktualnej granicy 672 stron i rezerwie 64, wymagany kandydat nie przekroczy 736 stron.

## Wymagana kolejność

Wzrost musi odbywać się wyłącznie przed pierwszym wywołaniem `ExitBootServices`.

1. Pobrać aktualną mapę firmware do wcześniej zarezerwowanego bufora.
2. Znormalizować mapę do `VGK_MEMORY_MAP_ENTRY`; nie alokować pamięci w normalizatorze.
3. Zaktualizować `boot_info.memory_map_size` i bieżący adres/rozmiar puli.
4. Zweryfikować cały `VGK_BOOT_INFO`, ACPI RSDP i pozostałe warunki wejściowe.
5. Obliczyć wymagania wspólnym preflightem i porównać `total_pages` z bieżącą pulą.
6. Jeżeli pula jest wystarczająca, przejść bezpośrednio do `ExitBootServices` z kluczem tej samej mapy. Pomiędzy `GetMemoryMap` i `ExitBootServices` nie może być żadnej usługi firmware ani alokacji.
7. Jeżeli pula jest za mała i `ExitBootServices` nie było jeszcze wywołane:
   - wyznaczyć nowy rozmiar przez `vgk_paging_pool_next_size()`;
   - przydzielić nową pulę jako `EfiLoaderData`, nie zwalniając starej;
   - sprawdzić wyrównanie, overflow i fizyczny zakres nowego adresu;
   - wyzerować wszystkie strony nowej puli;
   - zwolnić starą pulę;
   - dopiero wtedy zaktualizować `boot_info.page_table_pool_base/pages`;
   - wrócić do kroku 1 i ponownie pobrać mapę oraz policzyć wymagania. Adres nowej puli może zmienić liczbę tablic stron.
8. Jeżeli przydział nowej puli się nie powiedzie, pozostawić starą pulę nienaruszoną, zakończyć rozruch kontrolowanym błędem i zwolnić zasoby tylko przed pierwszą próbą `ExitBootServices`.
9. Jeżeli zwolnienie starej puli nie powiedzie się, nie przechodzić do startu jądra. Spróbować zwolnić nową pulę i zakończyć rozruch błędem.
10. Jeżeli `ExitBootServices` zwróci `EFI_INVALID_PARAMETER`, wolno tylko ponownie pobrać i znormalizować mapę, zweryfikować protokół/preflight i ponowić usługę w ramach limitu czterech prób. Nie wolno już alokować, zwalniać, zamykać protokołów ani wykonywać innych usług firmware.
11. Jeżeli po nieudanej próbie `ExitBootServices` nowa mapa wymaga większej puli, zatrzymać system. Nie próbować wzrostu po pierwszym wywołaniu tej usługi.

## Liczniki i stany

Licznik wzrostu i licznik prób `ExitBootServices` muszą być oddzielne. W przeciwnym razie próby powiększenia mogłyby zużyć limit ponowień `ExitBootServices` albo nieudana próba tej usługi mogłaby przypadkowo zezwolić na alokację.

Proponowany stan sterujący:
- `growth_steps`: początkowo 0, maksymalnie 4;
- `exit_attempts`: początkowo 0, maksymalnie 4;
- `exit_boot_services_started`: początkowo false; po pierwszym wywołaniu `ExitBootServices` pozostaje true niezależnie od wyniku.

Każdy przebieg pętli zaczyna się od świeżego `GetMemoryMap`. Klucz mapy nigdy nie może być użyty po `AllocatePages` lub `FreePages`.

## Rozdzielenie błędów

Helper preflightu nie powinien zwracać wyłącznie wartości logicznej. Loader musi rozróżnić:
- nieprawidłowe dane/protokół/ACPI;
- poprawne wymagania, ale zbyt małą pulę;
- wystarczającą pulę.

Tylko drugi przypadek przed pierwszym `ExitBootServices` może uruchomić wzrost. Błędu walidacji nie wolno próbować naprawiać przez przydzielenie większej puli.

## Warunki do sprawdzenia przed implementacją

- Dodać jawny test braku nakładania puli z ACPI RSDP; obecny walidator protokołu sprawdza nakładanie puli z jądrem, mapą, `VGK_BOOT_INFO` i framebufferem, ale nie ma osobnego warunku dla RSDP. UEFI AllocatePages powinno omijać pamięć ACPI, jednak kontrakt protokołu powinien to weryfikować.
- Potwierdzić kontrolę fizycznej szerokości adresu dla nowej puli przed przekazaniem jej jądru.
- Zachować stałe bufory mapy firmware i mapy znormalizowanej; `EFI_BUFFER_TOO_SMALL` nie jest powodem do powiększania puli tablic.
- Nie zmieniać obecnego retry `ExitBootServices` bez testów mockujących zmianę klucza i mapy.

## Wymagane testy

1. Początkowa pula wystarcza — brak alokacji i zwalniania.
2. Pula za mała — nowa jest przydzielona, wyzerowana, stara zwolniona, a wymagania są liczone ponownie.
3. Nieudany `AllocatePages` — stara pula pozostaje wybrana; jądro nie jest uruchamiane.
4. Nieudany `FreePages` starej puli — start przerwany; brak użycia nowej puli.
5. Nieprawidłowy adres, wyrównanie lub overflow nowej puli — kontrolowany błąd.
6. Nowy adres puli zwiększa wymagania — kolejny preflight i ewentualny drugi wzrost.
7. Osiągnięcie limitu 1024 lub limitu czterech wzrostów — kontrolowany błąd.
8. Zmiana mapy po alokacji/zwolnieniu — stare wymagania i stary klucz nie są używane.
9. `ExitBootServices` zwraca `EFI_INVALID_PARAMETER` — ponowny `GetMemoryMap` bez jakiejkolwiek alokacji/zwalniania.
10. Po nieudanej próbie `ExitBootServices` preflight wykazuje niedobór — natychmiastowe zatrzymanie, bez wzrostu.
11. `EFI_BUFFER_TOO_SMALL` i przekroczenie pojemności znormalizowanej mapy — błąd, nie wzrost puli.
12. Końcowa mapa i jej klucz są użyte bezpośrednio w `ExitBootServices`.

Najlepiej wydzielić sterowanie wzrostem do testowalnego komponentu z atrapami `AllocatePages`/`FreePages`; QEMU/OVMF pozostanie testem integracyjnym, nie zastąpi testów wszystkich ścieżek błędów.

## Warunek zakończenia etapu

Kod wzrostu jest już przygotowany w PR #28, ale nie należy go scalać przed:
- dodaniem testów z atrapami dla nieudanego `AllocatePages`, `FreePages`, nieprawidłowego adresu i overflow;
- przetestowaniem retry `ExitBootServices` z błędnym kluczem oraz potwierdzeniem, że po pierwszym wywołaniu nie ma alokacji ani zwalniania;
- pełnym audytem zależności PR #24–#28;
- osobnym testem z pendrive'a na fizycznym komputerze.
