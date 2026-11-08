# Plan bezpiecznego uruchamiania własnych tablic stron

## Cel

Następnym etapem po protokole startowym v2 jest przejęcie przez jądro kontroli nad mapowaniem pamięci w procesorze x86-64. Sam opis praw segmentów w protokole nie zmienia jeszcze uprawnień sprzętowych. Dopiero poprawne przygotowanie i aktywowanie tablic stron pozwoli rozdzielić pamięć wykonywalną od zapisywalnej.

Ten dokument opisuje kolejność prac i warunki bezpieczeństwa. Nie oznacza, że tablice stron są już zaimplementowane.

## Stan wejściowy

Obecny ładowacz UEFI przekazuje jądru:

- znormalizowaną mapę pamięci VGK;
- fizyczny zakres całego obrazu jądra i adres wejścia;
- od 1 do 16 opisów segmentów ELF;
- flagi odczytu, zapisu i wykonania każdego segmentu;
- wersję i rozmiar protokołu, które jądro może sprawdzić.

Walidator odrzuca segmenty W+X, nieprawidłowy punkt wejścia, nakładające się zakresy stron i niezerowe niewykorzystane pola. Wersja 2 protokołu jest testowana w CI.

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

## Docelowe uprawnienia segmentów

| Obszar | Odczyt | Zapis | Wykonanie |
|---|---:|---:|---:|
| Kod jądra | tak | nie | tak |
| Dane tylko do odczytu | tak | nie | nie |
| Dane i BSS | tak | tak | nie |
| Stos jądra | tak | tak | nie |
| Strony tablic stron | tak | tak | nie |

Jeżeli dwa segmenty wymagają różnych uprawnień, nie mogą współdzielić tej samej strony 4 KiB. Protokół v2 już odrzuca nakładanie się zaokrąglonych zakresów segmentów, co pozwala zachować ich oddzielne uprawnienia.

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
