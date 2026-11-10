# Plan bezpiecznego uruchamiania własnych tablic stron

## Cel

Następnym etapem po protokole startowym v4 jest przejęcie przez jądro kontroli nad mapowaniem pamięci w procesorze x86-64. Sam opis praw segmentów w protokole nie zmienia jeszcze uprawnień sprzętowych. Dopiero poprawne przygotowanie i aktywowanie tablic stron pozwoli rozdzielić pamięć wykonywalną od zapisywalnej.

Ten dokument opisuje kolejność prac i warunki bezpieczeństwa.

## Aktualny stan implementacji

Dodano eksperymentalny moduł budowania nieaktywnych tablic stron:
- interfejs: `include/vgk_paging.h`;
- implementacja: `src/paging.c`;
- test hostowy: `tests/test_paging.c`;
- cel testowy: `make paging-test`, uruchamiany także w GitHub Actions.

Moduł tworzy czteropoziomowe mapowanie stron 4 KiB w pamięci przekazanej przez wywołującego. Weryfikuje wyrównanie i kanoniczność adresów w modelu 48-bitowym, szerokość adresu fizycznego, duplikaty mapowań, brak pamięci oraz podstawowe uprawnienia stron. Odrzuca mapowanie W+X dla pojedynczego wpisu i ustawia NX dla stron niewykonywalnych, jeśli NX jest obsługiwane.

**Moduł nie jest jeszcze używany przez jądro.** Nie przełącza CR3, nie ustawia CR0.WP ani EFER.NXE i nie zmienia aktywnego mapowania. Pula stron musi być wyrównana, zarezerwowana i dostępna pod adresem tożsamościowym. Testy hostowe sprawdzają strukturę wpisów, a nie rzeczywistą ochronę sprzętową procesora. Testy obejmują mapowanie niskiego i wysokiego obszaru kanonicznego, ochronę NX, zapis, odrzucanie W+X, ponowne mapowanie tej samej strony, niekanoniczne adresy, adres fizyczny poza zadeklarowaną szerokością, brak obsługi NX i wyczerpanie puli stron.

Ważne ograniczenia do rozwiązania przed integracją:
- obecny builder zakłada czteropoziomowe stronicowanie i 48-bitowe adresy wirtualne; etap aktywacji musi wykryć i odrzucić niezgodny stan CR4.LA57;
- NX w wpisach wymaga wcześniejszego, bezpiecznego włączenia EFER.NXE; builder sam tego nie robi;
- W^X jest kontrolowane dla pojedynczego mapowania oraz dla aliasów: builder odrzuca ponowne mapowanie tej samej strony fizycznej z innymi uprawnieniami zapisu lub wykonania. Wskaźniki tabel pośrednich są dodatkowo sprawdzane względem aktualnie użytej części zarezerwowanej puli; kontrolowane są też dozwolone flagi wpisów i spójność licznika użytych stron, aby uszkodzone metadane nie prowadziły do dereferencji poza pulą. Testy obejmują również konflikt aliasów w odrębnej gałęzi PML4. Ochrona dotyczy obecnie stron 4 KiB i czteropoziomowych tablic tworzonych przez ten builder;
- builder nie przydziela stron samodzielnie z alokatora VGK i nie sprawdza jeszcze kolizji z wszystkimi strukturami startowymi;
- nie wykonano jeszcze testu przełączenia CR3 ani testu ochrony stron w QEMU.

Nie należy traktować tego modułu jako gotowego zarządcy pamięci wirtualnej.

## Stan wejściowy

Obecny ładowacz UEFI przekazuje jądru:

- znormalizowaną mapę pamięci VGK;
- fizyczny zakres całego obrazu jądra i adres wejścia;
- od 1 do 16 opisów segmentów ELF;
- flagi odczytu, zapisu i wykonania każdego segmentu;
- wersję i rozmiar protokołu, które jądro może sprawdzić.

Walidator odrzuca segmenty W+X, nieprawidłowy punkt wejścia, nakładające się zakresy stron i niezerowe niewykorzystane pola. Protokół startowy v4 jest testowany w CI.

## Zasady pierwszej wersji

1. Używać czteropoziomowych tablic stron x86-64 i stron 4 KiB.
2. Nie włączać jeszcze dużych stron 2 MiB ani 1 GiB.
3. Budować nowe tablice, nie modyfikować w ciemno tablic pozostawionych przez firmware.
4. Każdy nowy obszar tablic stron musi pochodzić z zarządcy pamięci fizycznej i zostać wyzerowany przed użyciem.
5. Sprawdzić w CPUID szerokość adresu fizycznego i obsługę bitu NX. Nie ustawiać NX, jeśli procesor go nie obsługuje; bezpieczniej przerwać start niż udawać ochronę.
6. Ustawić CR0.WP, aby także kod działający z poziomem uprzywilejowania jądra respektował ochronę przed zapisem do stron tylko do odczytu.
7. Nie aktywować nowych tablic, dopóki nie zostanie udowodnione, że nadal dostępne są wszystkie dane i instrukcje potrzebne do dokończenia przełączenia.

## Moduł rozpoznawania procesora

W kodzie znajduje się już pierwszy moduł CPUID: `include/vgk_cpu_features.h` i `src/cpu_features.c`. Odczytuje on maksymalne obsługiwane poziomy CPUID, sprawdza bit NX oraz odczytuje szerokość adresu fizycznego z rozszerzonego liścia `0x80000008`. Odrębna funkcja dekodująca pozwala testować te warunki bez wykonywania instrukcji CPUID.

Testy obejmują procesor bez NX, brak wymaganego liścia CPUID, niedopuszczalną szerokość adresu oraz prawidłowe granice 32 i 52 bitów. Moduł jest dołączony do natywnego jądra, a start testowy w QEMU ma potwierdzać wykrycie NX.

**Ważne:** wykrycie NX nie włącza jeszcze bitu NXE w rejestrze EFER i nie tworzy tablic stron. Na tym etapie jądro tylko rozpoznaje możliwości procesora i zapisuje diagnostykę. Włączenie ochrony nastąpi dopiero po przygotowaniu kompletnych mapowań i bezpiecznej obsługi błędów.

## Rozdzielenie kodu, danych tylko do odczytu i danych zapisywalnych\n\nNatywny plik linkera `linker-native.ld` rozdziela teraz segmenty ELF na trzy obszary: kod RX, dane stałe R+NX oraz dane/BSS RW+NX. Kontrola `tests/check_native_kernel.py` wymaga obecności osobnego segmentu tylko do odczytu i odrzuca nieznane flagi ELF oraz segmenty W+X. To przygotowuje dokładniejsze mapowanie stron, ale samo w sobie nie aktywuje ochrony sprzętowej.\n\n## Docelowe uprawnienia segmentów

| Obszar | Odczyt | Zapis | Wykonanie |
|---|---:|---:|---:|
| Kod jądra | tak | nie | tak |
| Dane tylko do odczytu | tak | nie | nie |
| Dane i BSS | tak | tak | nie |
| Stos jądra | tak | tak | nie |
| Strony tablic stron | tak | tak | nie |

Jeżeli dwa segmenty wymagają różnych uprawnień, nie mogą współdzielić tej samej strony 4 KiB. Protokół v4 już odrzuca nakładanie się zaokrąglonych zakresów segmentów, co pozwala zachować ich oddzielne uprawnienia.

## Najważniejszy warunek: ciągłość mapowania

Przed zmianą CR3 trzeba zidentyfikować i zachować mapowanie co najmniej:

- kodu i danych jądra;
- aktualnego stosu, z którego wykonywany jest kod przełączający;
- struktury VGK_BOOT_INFO;
- tablicy znormalizowanej mapy pamięci;
- nowych tablic stron;
- kodu i danych ładowacza, jeśli są jeszcze używane po skoku do jądra;
- innych struktur, do których jądro odwołuje się przed ustanowieniem własnego stosu.

Nie wolno zakładać, że struktura VGK_BOOT_INFO albo mapa pamięci znajdują się w obszarze jądra. W obecnej implementacji są to dane statyczne ładowacza UEFI. Ich fizyczne zakresy i czas życia muszą zostać jawnie uwzględnione. Trzeba też ustalić, czy most asemblerowy ustawia własny stos, czy korzysta ze stosu firmware.

Pierwsza wersja powinna zachować mapowanie tożsamościowe (adres wirtualny równy fizycznemu) tylko dla jawnie wybranych, potrzebnych zakresów. Nie należy mapować całej przestrzeni fizycznej jako zwykłej pamięci RAM: mogłoby to objąć obszary zarezerwowane, urządzenia i rejestry MMIO.

## Kolejność implementacji

1. Dodać odczyt CPUID i testy jego wyników. Ustalić bezpieczną reakcję na brak NX i ograniczoną szerokość adresu fizycznego.
2. Rozszerzyć informacje startowe o niezbędne zakresy pomocnicze, jeżeli nie można ich bezpiecznie wyznaczyć z mapy pamięci. Nie dodawać pól bez określonej semantyki i walidacji.
3. Przygotować funkcję budującą tablice stron bez ich aktywowania. Funkcja ma sprawdzać wyrównanie, kanoniczność adresów, przepełnienia i limity tablic.
4. Dodać testy hostowe dla indeksów poziomów, granic zakresów, konfliktów uprawnień, braku pamięci i mapowania segmentów.
5. Zweryfikować, że każda strona tabeli jest wyzerowana, znajduje się w pamięci dostępnej dla jądra i nie nadpisuje segmentu jądra ani struktur startowych.
6. W QEMU/OVMF najpierw zbudować tablice i je zweryfikować, nadal działając na dotychczasowym mapowaniu.
7. Dopiero w osobnym kroku przełączyć CR3, ustawić CR0.WP i sprawdzić diagnostycznie, czy kod nadal działa.
8. Dodać testy ochrony: zapis do kodu ma wywołać kontrolowany wyjątek, wykonanie strony NX ma zostać zablokowane, a poprawny kod i dane mają pozostać dostępne.
9. Dodać obsługę wyjątków i diagnostykę przed rozszerzaniem mapowania na urządzenia oraz pozostałe obszary sprzętowe.

## Warunki zatrzymania

Start należy przerwać w kontrolowany sposób, jeśli:

- procesor nie spełnia wymaganych warunków;
- mapa pamięci jest niespójna albo nie obejmuje potrzebnych struktur;
- nie można przydzielić lub wyzerować stron tablic;
- segmenty wymagają sprzecznych uprawnień;
- nie da się zagwarantować ciągłości mapowania stosu, kodu lub danych;
- kontrola tablic stron wykryje nieprawidłowy wpis.

Nie wolno w takiej sytuacji kontynuować startu z częściowo zbudowanymi tablicami.

## Ograniczenia i testy

- Test QEMU/OVMF nie zastępuje testu na fizycznych komputerach Intel i AMD.
- Pierwsza wersja nie będzie jeszcze pełnym zarządcą pamięci wirtualnej ani obsługą wszystkich urządzeń.
- Wdrożenie musi być etapowe: testy hostowe, test QEMU, kontrolowane testy awarii, dopiero później próby na sprzęcie.
- Nie wolno oznaczać tej funkcji jako gotowej, dopóki testy ochrony stron rzeczywiście nie potwierdzą zachowania procesora.

## Rozszerzone rozpoznawanie możliwości procesora

Moduł CPUID został rozszerzony o odczyt producenta, rodziny, modelu i rewizji procesora oraz wybranych możliwości SSE2, SSE3, SSSE3, SSE4.1, SSE4.2 i AVX/AVX2. Identyfikator producenta i model służą diagnostyce, a nie zamkniętej liście dozwolonych procesorów.

Szczególnie ważne jest rozróżnienie:
- **AVX obecne w procesorze** – procesor deklaruje instrukcje;
- **stan AVX udostępniony przez system** – procesor deklaruje OSXSAVE, a XCR0 ma włączony zapis i odtwarzanie stanu XMM/YMM;
- **AVX2 dostępne** – procesor deklaruje AVX2 i jednocześnie spełnione są warunki udostępnienia stanu AVX.

Instrukcja XGETBV jest wykonywana dopiero po sprawdzeniu OSXSAVE. Dzięki temu VGK nie uzna samej obecności AVX w układzie za zgodę na wykonywanie tych instrukcji. Na tym etapie jądro jedynie rozpoznaje możliwości i wypisuje diagnostykę; nie zawiera jeszcze ścieżek kodu wykorzystujących AVX2.

### Zasada zgodności z różnymi generacjami

- Jądro i kod startowy pozostają kompilowane dla bezpiecznego, podstawowego zestawu x86-64, bez wymuszania AVX/AVX2.
- Funkcje przyspieszone będą osobnymi wariantami, wybieranymi dopiero po sprawdzeniu sprzętu i stanu systemu.
- Brak nowszej instrukcji nie może uniemożliwić uruchomienia podstawowego systemu.
- Nieznany model procesora nie jest automatycznie odrzucany; znaczenie mają wymagane cechy architektury i poprawność działania.
- Każdy wariant przyspieszony będzie musiał mieć odpowiednik podstawowy i testy na emulatorze oraz rzeczywistym sprzęcie.

**Ograniczenie obecnego etapu:** wykrywanie możliwości nie oznacza jeszcze pełnej zgodności ze wszystkimi procesorami. Nie wykonaliśmy jeszcze testów na fizycznych komputerach Intel i AMD, a jądro nie przełącza się na kod AVX/AVX2.


## Konkretny plan pierwszego zestawu mapowań (bez przełączania CR3)

Przed integracją buildera jądro musi zbudować i sprawdzić mapowania w nowej, nadal nieaktywnej tablicy. Pierwszy zakres prac ograniczamy do pamięci RAM i struktur koniecznych do działania jądra:

1. **Segmenty ELF jądra:** mapować każdy segment pod jego adresem tożsamościowym. Uprawnienia wynikają z flag segmentu: RX dla kodu, R+NX dla danych stałych, RW+NX dla danych i BSS. Stos `vgk_boot_stack` znajduje się w BSS, więc nie wymaga osobnego obszaru, o ile walidacja ELF potwierdzi jego przynależność do segmentu RW.
2. **VGK_BOOT_INFO:** wyznaczyć zakres stron obejmujący cały obiekt, zaokrąglony w dół i w górę do granic 4 KiB; mapować R+NX.
3. **Znormalizowana mapa pamięci:** analogicznie mapować cały bufor wskazywany przez `memory_map_address` i `memory_map_size` jako R+NX. Przed mapowaniem sprawdzić dzielenie rozmiaru przez rozmiar wpisu i przepełnienia.
4. **Pula tablic stron:** przydzielić ją z alokatora VGK, zachować jej fizyczny zakres jako zarezerwowany i mapować RW+NX. Pamięć puli musi być dostępna tożsamościowo przed aktywacją nowych tablic, ponieważ builder zeruje i zapisuje wpisy przez wskaźniki.
5. **Pozostałe obszary:** na tym etapie nie mapować framebuffer GOP ani tabel ACPI. Wymagają osobnej polityki pamięci urządzeń/cache oraz parsera ACPI z walidacją długości i zakresów.

### Konflikty zakresów

Obiekty startowe mogą zajmować tę samą stronę 4 KiB, mimo że ich adresy bajtowe są różne. Plan mapowania powinien więc najpierw utworzyć listę zakresów stron, posortować ją po adresie i połączyć zakresy zachodzące na siebie lub sąsiadujące, jeżeli mają identyczne uprawnienia. Nakładanie zakresów o różnych uprawnieniach musi zakończyć się błędem przed rozpoczęciem mapowania. Nie wolno rozwiązywać konfliktu przez nadpisanie wcześniejszego wpisu PTE.

### Warunki przed testem przełączenia

- potwierdzić, że bieżący kod, stos, `VGK_BOOT_INFO`, mapa pamięci i pula są objęte nowymi mapowaniami;
- potwierdzić, że wszystkie adresy fizyczne mieszczą się w szerokości wykrytej przez CPUID;
- sprawdzić, że żaden zakres nie mapuje RAM jako MMIO ani odwrotnie;
- sprawdzić, że wszystkie strony puli zostały wyzerowane i nie należą do żadnego innego obiektu;
- przejść po zbudowanych tablicach i porównać każdy wpis z oczekiwaną listą zakresów oraz uprawnień;
- wykonać testy QEMU/OVMF z diagnostyką szeregową, nadal bez zmiany CR3.

Dopiero osobny etap może dodać procedurę aktywacji. Musi ona działać z kodu i stosu już objętych nowymi mapowaniami, zachować mapowanie adresu powrotu i danych, ustawić NXE tylko po sprawdzeniu CPUID, ustawić CR0.WP i przeładować CR3. Testy awarii ochrony (zapis do strony RX i wykonanie strony NX) wymagają wcześniej działającej obsługi wyjątku #PF; bez niej próba naruszenia ochrony może zakończyć się potrójnym błędem procesora i resetem maszyny.

## Korekta dotycząca puli tablic stron po analizie ładowacza UEFI

Analiza `boot/uefi/bootx64.c` wykazała, że ładowacz pobiera końcową mapę pamięci dopiero po załadowaniu segmentów jądra i zamknięciu woluminu. Bufor surowej mapy, znormalizowana mapa VGK oraz `VGK_BOOT_INFO` są obecnie statycznymi obiektami obrazu ładowacza. Znormalizowana mapa klasyfikuje obszary UEFI LoaderCode/LoaderData jako `VGK_MEMORY_BOOTLOADER`, a więc nie jako pamięć wolną dla alokatora jądra.

Nie wolno jednak wyciągać z tego wniosku, że dowolny adres zwrócony przez alokator po `ExitBootServices` jest bezpieczny do dereferencji. Alokator operuje na fizycznej mapie RAM, natomiast builder tablic stron zapisuje do puli przez wskaźniki. Sam przydział fizyczny nie dowodzi, że firmware pozostawiło tę stronę w aktywnym mapowaniu tożsamościowym. Obecny test alokatora, który przydziela i zwalnia stronę bez odczytu/zapisu pod tym adresem, tego nie sprawdza.

### Decyzja projektowa dla pierwszego bezpiecznego przełączenia

Puli tablic stron nie będziemy przydzielać po starcie jądra z ogólnego alokatora fizycznego. **Na gałęzi rozwojowej wdrożono kontrakt v4 i rezerwację UEFI**: ładowacz przydziela ją przez `AllocatePages(EfiLoaderData)` przed pobraniem końcowej mapy pamięci i przed pierwszą próbą `ExitBootServices`, zeruje ją jeszcze podczas działania Boot Services i przekazuje jądru jej adres fizyczny oraz liczbę stron. Pełna poprawność tej ścieżki pozostaje do potwierdzenia kompilacją, testami i uruchomieniem QEMU/OVMF; nie oznaczamy jej jeszcze jako zweryfikowanej.

Wymaga to kontrolowanej zmiany protokołu startowego:
- podnieść wersję `VGK_BOOT_INFO` z 3 do 4;
- dodać jawne pola adresu i rozmiaru puli tablic stron;
- walidować wyrównanie, niezerowy rozmiar, przepełnienia i zgodność zakresu z mapą pamięci;
- wymagać, aby zakres puli był sklasyfikowany jako pamięć ładowacza, a nie pamięć używalna;
- zarezerwować ten zakres w kontrakcie startowym i używać go wyłącznie jako puli RW+NX;
- dodać testy walidatora protokołu, normalizacji mapy i błędnych zakresów puli;
- zapewnić, że pula nie zostanie zwolniona przed przejściem jądra na własne mapowanie.

Rezerwację należy wykonać przed końcowym `GetMemoryMap`, ponieważ `AllocatePages` zmienia mapę i unieważnia poprzedni klucz mapy. Po przydziale nie wolno już wykonywać operacji alokujących pamięć pomiędzy końcowym pobraniem mapy a udanym `ExitBootServices`; istniejąca pętla ponawiania może nadal ponawiać wyłącznie pobranie mapy i samo wyjście.

W pierwszej wersji rozmiar puli powinien być stałą, konserwatywną wartością określoną przez testy (np. 64 strony), a brak lub niewystarczający rozmiar ma zatrzymać start z diagnostyką. Nie wolno po cichu wracać do przydzielania puli z alokatora jądra. W przyszłości można dodać obliczanie wymaganego rozmiaru na podstawie planu mapowań.

### Zakres tej decyzji

To rozwiązanie dotyczy natywnego ładowacza UEFI. Dla Legacy BIOS trzeba zaprojektować równoważny kontrakt rezerwacji pamięci, zanim wspólne jądro będzie mogło bezpiecznie używać buildera na obu ścieżkach. Nie aktywujemy CR3 w ramach samej zmiany protokołu. Najpierw należy wdrożyć i przetestować przekazanie puli, a następnie zbudować i zweryfikować nieaktywne tablice w QEMU/OVMF.

