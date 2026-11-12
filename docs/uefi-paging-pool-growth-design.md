# Projekt: dynamiczna pula tablic stron UEFI

## Cel i zakres

Celem następnego etapu jest umożliwienie programowi rozruchowemu UEFI dobrania puli tablic stron do rzeczywistego zapotrzebowania jądra, zamiast bezwarunkowego używania 64 stron. Ten dokument opisuje projekt; **nie zmienia jeszcze alokacji, protokołu startowego ani zachowania jądra**.

Obecna kontrola pojemności w jądrze działa po `ExitBootServices`. Wykrywa niedobór bezpiecznie, zanim rozpocznie budowanie tablic, ale nie może już poprosić firmware o większą pulę. Sam kalkulator wymagań jest obecnie dołączany do obrazu jądra, a nie do programu UEFI.

## Ustalenia z audytu

1. `boot/uefi/bootx64.c` rezerwuje obecnie dokładnie `VGK_BOOT_PAGE_TABLE_POOL_PAGES = 64` stron przed końcowym pobraniem mapy pamięci.
2. Każde `AllocatePages` i `FreePages` może zmienić mapę pamięci UEFI. Po zmianie trzeba ponownie pobrać mapę, znormalizować ją i użyć świeżego klucza mapy przy `ExitBootServices`.
3. Fizyczny adres nowej puli może zmienić liczbę struktur tablic stron. Wymagania trzeba więc liczyć ponownie po każdej zmianie adresu lub rozmiaru puli.
4. `vgk_boot_info_is_valid()` wymaga obecnie dokładnie 64 stron. `page_table_pool_is_zeroed()` w jądrze także wymaga dokładnie 64 stron. Oba ograniczenia muszą zostać zmienione razem, a nie osobno.
5. Kalkulator `src/paging_requirements.c` jest obecnie linkowany do jądra. Aby program UEFI mógł podjąć decyzję przed `ExitBootServices`, potrzebuje tej samej, przetestowanej implementacji jako współdzielonego modułu freestanding lub równoważnego wspólnego komponentu. Nie należy tworzyć drugiej, rozbieżnej implementacji liczenia.
6. Końcowa mapa UEFI musi opisywać już ostateczną pulę. Po jej pobraniu nie wolno wykonywać żadnych alokacji ani zwolnień przed wywołaniem `ExitBootServices`.
7. Po nieudanym `ExitBootServices` wolno ponowić próbę wyłącznie zgodnie z istniejącym ograniczeniem: nie zwalniać pamięci, nie zamykać protokołów, nie drukować i nie wykonywać innych usług, które mogłyby zmienić mapę. Ponowne pobranie mapy i próba z nowym kluczem muszą pozostać ograniczone i przewidywalne.

## Proponowany przepływ

1. Załadować i zweryfikować jądro, segmenty ELF oraz pozostałe dane startowe.
2. Przydzielić początkową pulę stron i ją wyzerować.
3. Pobrać mapę firmware i znormalizować ją do bufora VGK.
4. Zbudować zakresy wirtualne identyczne z tymi, które później mapuje jądro: segmenty ELF, strukturę `VGK_BOOT_INFO`, znormalizowaną mapę, samą pulę, zweryfikowany ACPI RSDP i framebuffer tylko wtedy, gdy polityka pamięci podręcznej gwarantuje mapowanie UC.
5. Obliczyć wymagania wspólnym kalkulatorem. Wynik musi uwzględniać bieżący fizyczny adres i rozmiar puli.
6. Jeżeli pula jest za mała, użyć przetestowanej polityki wzrostu z `vgk_paging_pool_next_size()`. Najpierw przydzielić nową pulę; dopiero po sukcesie zwolnić starą. Wyzerować nową pulę, zaktualizować pola protokołu i wrócić do kroku 3.
7. Każda iteracja musi zwiększać rozmiar puli, mieć jawny limit prób i twardy limit liczby stron. Przy błędzie alokacji, przekroczeniu limitu, błędzie kalkulatora lub braku postępu zakończyć start kontrolowanym błędem.
8. Gdy wymagania mieszczą się w puli, zamknąć wolumin i wszystkie czynności/protokoły, które muszą być zamknięte przed końcową mapą.
9. Pobrać końcową mapę do już zarezerwowanego bufora, znormalizować ją, ustawić rozmiar mapy w protokole, zweryfikować cały `VGK_BOOT_INFO` i natychmiast wywołać `ExitBootServices` z kluczem tej mapy.
10. Jeżeli klucz jest nieaktualny, ponowić tylko końcowe pobranie mapy i wywołanie `ExitBootServices` w ramach istniejącego, małego limitu prób. Nie zmieniać już puli.

## Ważne warunki bezpieczeństwa

- Nie zwalniać starej puli przed udanym przydzieleniem nowej.
- Nie używać starego wyniku kalkulatora po zmianie adresu puli, rozmiaru puli lub mapy.
- Nie wykonywać alokacji po pobraniu mapy przeznaczonej bezpośrednio dla `ExitBootServices`.
- Nie zwiększać puli bez twardego limitu i kontroli przepełnień.
- Nie zmieniać `CR3`, `CR0.WP` ani `EFER.NXE` w tym etapie.
- Nie traktować powodzenia testów QEMU/OVMF jako potwierdzenia startu na fizycznym komputerze.

## Zmiany wymagane przed integracją

- Współdzielić kalkulator wymagań z programem UEFI i jądrem; dodać test kompilacji oraz linkowania obu ścieżek.
- Ustalić i uzasadnić maksymalny rozmiar puli oraz maksymalną liczbę iteracji. Nie wpisywać tych limitów arbitralnie.
- Rozszerzyć testy protokołu startowego o dozwolone rozmiary puli, wyrównanie, mnożenie przez rozmiar strony, przepełnienie, limit maksymalny i kolizje zakresów.
- Zmienić sprawdzanie wyzerowania puli i wszystkie miejsca, które zakładają stałe 64 strony.
- Dodać testowalny komponent sterujący iteracjami albo testy UEFI z atrapami usług, aby sprawdzić sukces alokacji, nieudaną alokację, zmianę adresu, zmianę mapy, osiągnięcie limitu i brak postępu.
- Zachować końcową sekwencję `GetMemoryMap` / `ExitBootServices` jako osobny, minimalny fragment kodu.

## Status

To jest dokument projektowy i wynik audytu kodu. Nie wprowadza jeszcze dynamicznej alokacji. Aktualna pula pozostaje 64-stronicowa, a dotychczasowy preflight jądra nadal odrzuca niedostateczną pojemność przed budowaniem tablic stron.
