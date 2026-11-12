# Projekt: dynamiczne dobieranie puli tablic stron

Status: projekt techniczny — **niezaimplementowany**. Obecny rozruch nadal używa stałej puli 64 stron (256 KiB). Ten dokument nie zmienia protokołu startowego ani kodu rozruchowego.

## 1. Cel i zasady bezpieczeństwa

Pula ma być dobierana do rzeczywistego zapotrzebowania jądra, zamiast zakładać, że 64 strony wystarczą dla każdego obrazu ELF. Mechanizm musi:

- obliczyć zapotrzebowanie przed `ExitBootServices`, gdy UEFI nadal pozwala przydzielać i zwalniać strony;
- uwzględnić adres i pełny rozmiar kandydującej puli, ponieważ sama pula jest mapowana tożsamościowo;
- ponowić obliczenia po każdej zmianie alokacji i po pobraniu nowej mapy pamięci;
- nigdy nie przekazać jądru puli mniejszej niż obliczone zapotrzebowanie;
- zakończyć start kontrolowanym błędem, jeśli nie da się uzyskać wystarczającej puli;
- zachować obecne zasady: bez aktywowania CR3, CR0.WP i EFER.NXE w tym etapie.

Nie wolno traktować sukcesu QEMU/OVMF jako potwierdzenia rozruchu na fizycznym komputerze.

## 2. Dane wejściowe kalkulatora

Kalkulator z `include/vgk_paging_requirements.h` i `src/paging_requirements.c` powinien pozostać jednym, wspólnym źródłem obliczeń dla testów, programu UEFI i jądra. Należy skompilować go również do obrazu UEFI, bez tworzenia drugiej implementacji algorytmu.

Do obliczeń należy przekazać zakresy tożsamościowo mapowane przez jądro:

1. wszystkie segmenty ELF — z pełnym `memory_size`, także BSS;
2. strukturę `VGK_BOOT_INFO`;
3. znormalizowaną mapę UEFI o rozmiarze przekazanym w protokole;
4. całą pulę tablic stron, łącznie z obszarem zapasu;
5. RSDP ACPI, jeśli jest zgłoszony;
6. framebuffer, jeśli jest zgłoszony.

Dla RSDP i framebufferu etap UEFI może użyć konserwatywnego zakresu, nawet jeśli późniejsza walidacja lub polityka pamięci podręcznej spowoduje pominięcie mapowania przez jądro. Nadmiarowe oszacowanie jest bezpieczniejsze niż niedoszacowanie. Jądro nadal wykonuje własną walidację i końcowy preflight na podstawie faktycznie mapowanych zakresów.

Stos nie jest dodawany osobno, jeżeli pozostaje zawarty w mapowanym segmencie ELF.

## 3. Pętla doboru rozmiaru

Proponowany przebieg przed końcowym pobraniem mapy:

1. Załaduj i zweryfikuj kernel tak jak obecnie.
2. Przydziel początkową pulę-kandydata (na początku 64 strony, aby zachować dotychczasowy punkt odniesienia).
3. Wyzeruj całą pulę i wpisz jej adres oraz liczbę stron do struktury startowej.
4. Pobierz aktualną mapę UEFI i znormalizuj ją do istniejącego formatu VGK.
5. Zbuduj listę zakresów mapowanych przez jądro, uwzględniając aktualny adres i pełny rozmiar puli.
6. Uruchom wspólny kalkulator.
7. Jeżeli `required_pages <= candidate_pages`, kandydat jest wystarczający i można przejść do procedury końcowej.
8. Jeżeli pula jest za mała, wyznacz większy kandydat z zapasem, zwolnij starą pulę, przydziel nową, wyzeruj ją i wróć do kroku 4. Nowy adres może zmienić liczbę potrzebnych struktur, więc starego wyniku nie wolno ponownie używać.
9. Po osiągnięciu limitu iteracji, limitu rozmiaru albo błędzie alokacji przerwij start bez przekazywania niepełnej puli.

Wielkość kandydata powinna rosnąć co najmniej do wartości wymaganej przez kalkulator oraz zawierać jawny zapas. Dokładny współczynnik zapasu i limit maksymalny muszą zostać ustalone na podstawie testów i ograniczeń pamięci, a nie przyjęte bez pomiarów. Nie należy na tym etapie wpisywać arbitralnego limitu jako gwarantowanej wartości dla wszystkich komputerów.

## 4. Krytyczna granica ExitBootServices

Po pierwszym wywołaniu `ExitBootServices` nie wolno wykonywać alokacji ani zwalniania pamięci UEFI. Obecna zasada kodu — po nieudanej próbie dopuszczalne są wyłącznie kolejne pobranie mapy i ponowienie `ExitBootServices`, a przy innych błędach zatrzymanie procesora — musi pozostać zachowana.

Dlatego:

- wszystkie zmiany rozmiaru puli muszą zakończyć się przed pierwszym `ExitBootServices`;
- bezpośrednio przed pierwszą próbą należy pobrać świeżą mapę i ponownie policzyć wymagania;
- jeżeli próba `ExitBootServices` zwróci `EFI_INVALID_PARAMETER`, wolno ponownie pobrać i znormalizować mapę oraz sprawdzić, czy istniejąca pula nadal wystarcza;
- jeśli po nieudanej próbie nowa mapa wymaga większej puli, nie wolno jej już powiększać — należy bezpiecznie zatrzymać start. Zapas ma ograniczać prawdopodobieństwo takiej sytuacji, ale nie może być przedstawiany jako matematyczna gwarancja wobec dowolnych zmian firmware.

Nie wolno wykonywać dodatkowych operacji UEFI pomiędzy ostatnim `GetMemoryMap` a odpowiadającym mu `ExitBootServices`, ponieważ mogłyby unieważnić klucz mapy.

## 5. Zmiana protokołu startowego

Dopiero po przetestowaniu algorytmu należy zmienić walidację `VGK_BOOT_INFO`:

- zastąpić wymóg dokładnie 64 stron bezpiecznym zakresem od minimalnej liczby do ustalonego limitu;
- sprawdzać wyrównanie, mnożenie liczby stron przez 4096, koniec zakresu i wszystkie kolizje z segmentami ELF, mapą pamięci, `VGK_BOOT_INFO`, framebufferem i innymi zarezerwowanymi obszarami;
- nie zmieniać znaczenia istniejącego pola `page_table_pool_pages`;
- podnieść wersję protokołu tylko wtedy, gdy zmieni się jego kontrakt lub kompatybilność, a następnie zaktualizować testy i wszystkie implementacje producenta/konsumenta;
- zachować kontrolę, że pula znajduje się w pamięci typu bootloader.

Sama zmiana liczby stron w istniejącym polu nie musi wymagać nowej wersji protokołu, jeżeli nowa wersja walidatora jawnie dopuszcza zakres i układ struktury pozostaje identyczny. Decyzję trzeba podjąć po audycie wszystkich konsumentów protokołu.

## 6. Testy wymagane przed integracją

### Testy czystego planera
- zapotrzebowanie mniejsze, równe i większe od kandydata;
- wzrost rozmiaru puli oraz przekroczenie limitu;
- limit iteracji i brak postępu;
- zmiana adresu puli między iteracjami, która zmienia wynik kalkulatora;
- przepełnienia arytmetyczne i nieprawidłowe dane;
- brak alokacji po przejściu do fazy ExitBootServices.

### Testy z atrapami usług UEFI
- alokacja, zwolnienie i ponowna alokacja zmieniają mapę;
- zmiana puli wymusza ponowne GetMemoryMap i ponowne obliczenie;
- błąd alokacji kończy się bez użycia starej, zwolnionej puli;
- bufor mapy za mały lub niepoprawny deskryptor kończy się kontrolowanym błędem;
- po pierwszej nieudanej próbie ExitBootServices nie występuje AllocatePages ani FreePages;
- ponowienie ExitBootServices korzysta z klucza z bezpośrednio poprzedzającego GetMemoryMap;
- jeżeli nowa mapa wymaga więcej stron niż przydzielono, start jest zatrzymany, a nie kontynuowany.

### Testy integracyjne
- obecny przypadek mieszczący się w 64 stronach;
- obraz ELF wymagający więcej niż 64 stron;
- obraz wymagający więcej niż dopuszczalny limit;
- segment BSS zwiększający zapotrzebowanie;
- wysokie adresy kanoniczne, framebuffer i ACPI;
- QEMU/OVMF, testy statyczne i wszystkie dotychczasowe testy protokołu.

## 7. Kolejność wdrożenia

1. Dodać i przetestować czysty planer iteracji bez zmiany ścieżki startowej.
2. Dodać atrapy usług UEFI i testy kolejności wywołań.
3. Zintegrować wspólny kalkulator z programem UEFI.
4. Dodać walidację zmiennego rozmiaru puli do protokołu.
5. Dopiero po pełnym CI uruchomić nową ścieżkę w QEMU/OVMF.
6. Zachować dotychczasową ścieżkę 64 stron jako punkt odniesienia w testach; nie scalać zmian bez przeglądu.

## 8. Kryterium ukończenia

Etap będzie gotowy dopiero wtedy, gdy testy udowodnią, że żadna ścieżka nie przekazuje jądru za małej puli, żadne wywołanie alokacji/zwalniania nie występuje po pierwszej próbie `ExitBootServices`, a wszystkie dotychczasowe testy i rozruch QEMU/OVMF przechodzą. Do tego czasu stała pula 64 stron pozostaje aktywnym, bezpiecznym wariantem.
