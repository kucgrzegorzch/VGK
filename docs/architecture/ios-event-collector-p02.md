# IOS P0.2 – prototyp kolektora zdarzeń

**Status:** prototyp host-side do testów. Nie jest częścią działającego jądra VGK, nie jest usługą systemową i nie stanowi produkcyjnej granicy bezpieczeństwa.

## Cel

Sprawdzić podstawowe zachowanie kolektora przed podłączeniem jakichkolwiek rzeczywistych źródeł VGK:
- rejestrację dozwolonych źródeł i kodów zdarzeń;
- walidację wersji, pól, kategorii, ważności, pewności i danych payload;
- odrzucanie wiadomości z więcej niż siedmioma polami na najwyższym poziomie przed iteracją po kluczach;
- ograniczenie rozmiaru i częstotliwości komunikatów;
- wstępne ograniczenie struktury i rozmiaru danych odbiornika przed serializacją JSON; Kontrola długości UTF-8 liczy bajty bez tworzenia tymczasowej kopii zakodowanego tekstu i kończy się po przekroczeniu budżetu.
- kolejkę o stałej pojemności;
- numerację kanoniczną nadawaną przez kolektor;
- ograniczone oznaczanie możliwych powtórzeń na podstawie numeru źródłowego lub odcisku treści;
- jawne liczenie odrzuceń spowodowanych limitem;
- przekazywanie rekordów do odbiornika pamięciowego bez utraty wpisu, gdy odbiornik odmawia zapisu.

## Pliki

- `tools/ios_event_collector.py` – implementacja prototypu.
- `tests/test_ios_event_collector.py` – testy jednostkowe.
- `Makefile` – niezależny cel `ios-monitor-test`.

## Jawne limity pamięci i katalogów

Domyślne granice prototypu P0.2:
- maksymalnie 64 zarejestrowane źródła;
- maksymalnie 128 kodów zdarzeń na jedno źródło;
- maksymalnie 4096 kodów zdarzeń łącznie we wszystkich źródłach;
- `source_sequence` oraz lokalny `sequence` odbioru mieszczą się w zakresie bez znaku 64-bitowego: `0..2^64-1`;
- kolejka: maksymalnie 4096 rekordów;
- indeks możliwych powtórzeń: maksymalnie 8192 klucze;
- odbiornik pamięciowy: maksymalnie 4096 rekordów, każdy do 8192 bajtów JSON;
- źródło: maksymalnie 10000 zdarzeń w oknie nie dłuższym niż 3600 sekund;
- licznik odrzuceń nasyca się na wartości uint64 i nie rośnie bez ograniczeń.

Wszystkie limity konfiguracyjne można obniżać, ale nie można ich podwyższać ponad powyższe wartości maksymalne. Wartości nie-dodatnie, niecałkowite i logiczne są odrzucane. Rekord odbiornika musi być skończonym JSON-em i mieścić się w limicie bajtowym. Przekroczenie limitu rejestracji jest jawnie odrzucane; istniejące wpisy nie są usuwane ani zastępowane. Po wyczerpaniu 64-bitowej numeracji kolektor zgłasza `SequenceExhausted` i nie zawija licznika.

## Uruchomienie testów

```sh
make ios-monitor-test
```

Test jest niezależny od kompilacji jądra. Nie jest uruchamiany przez domyślne `make test`, aby nie zmieniać znaczenia istniejącego testu jądra.

## Granice i ograniczenia

1. **Brak integracji z jądrem.** Moduł działa wyłącznie jako kod Pythona uruchamiany na hoście.
2. **Brak uprawnień systemowych.** Prototyp nie otwiera urządzeń, nie odczytuje pamięci jądra i nie zmienia plików systemowych.
3. **Model wykonania jest jednowątkowy.** Prototyp nie implementuje synchronizacji współbieżnego dostępu; testy wywołują kolektor sekwencyjnie.
4. **Rejestr źródeł jest symulacją.** Uchwyt obiektu działa wyłącznie w obrębie procesu testowego. Python nie zapewnia tu izolacji przed złośliwym kodem w tym samym procesie. Nie jest to uwierzytelnianie procesów, sterowników ani sprzętu.
5. **Odbiornik jest ulotny.** Odbiornik pamięciowy przechowuje rekordy wyłącznie w pamięci i nie zapewnia trwałości po zakończeniu procesu lub restarcie.
6. **Wykrywanie możliwych powtórzeń nie usuwa zdarzeń.** Przy obecnym `source_sequence` prototyp oznacza ponowne użycie numeru w obrębie źródła, sesji i kodu zdarzenia. Gdy numeru brak, porównuje SHA-256 kanonicznej treści (wersja źródła, kod, kategoria, ważność, pewność i payload). Dla źródeł z numeracją porównanie treści nie jest używane: dwa różne numery oznaczają odrębne zgłoszenia, nawet jeśli ich pozostała treść jest identyczna. Odcisk jest wyłącznie wskazówką: identyczna treść nie dowodzi, że chodzi o to samo zdarzenie. Każde poprawne zgłoszenie otrzymuje własny numer odbioru i trafia do kolejki, również wtedy, gdy wygląda na powtórzenie. Pole `duplicate_detection` zawiera status, identyfikator wcześniejszego możliwego dopasowania i odcisk. Indeks przechowuje jeden klucz na przyjęte zdarzenie i jest ograniczony pamięciowo, więc bardzo stare powtórzenie może nie zostać skojarzone. Nie ma automatycznego odrzucania na podstawie podobieństwa ani ponownego użycia numeru źródłowego.
7. **Czas ścienny jest niedostępny.** Prototyp zapisuje `timestamp: null` i `time_status: unavailable`; osobno zachowuje `monotonic_time` do porządkowania zdarzeń w obrębie sesji. Nie deklaruje wiarygodnego czasu ściennego.
8. **Przepełnienie nie jest ukrywane.** Kolejka pozostaje ograniczona; odrzucone zdarzenia zwiększają licznik `dropped_count`. Prototyp nie udaje, że utracone zdarzenia zostały zapisane do dziennika.
9. **Brak działań ochronnych.** Moduł nie podejmuje decyzji polityki, nie blokuje, nie kwarantannuje, nie naprawia i nie usuwa żadnych danych.

## Kryteria P0.2

P0.2 może zostać oceniony dopiero po uruchomieniu testów w kontrolowanym środowisku. Testy obejmują poprawne przyjęcie zdarzenia, obcy uchwyt źródła, próbę podwyższenia ważności, nieznane pola i kody, oznaczanie możliwych powtórzeń bez utraty zdarzeń, przepełnienie kolejki, limit danych, limit częstotliwości, błąd odbiornika i zachowanie kolejności.

Pomyślne testy host-side nie dowodzą bezpieczeństwa mechanizmu w jądrze ani nie upoważniają do podłączenia rzeczywistych źródeł. Następny etap wymaga osobnego przeglądu kodu, wyników testów i decyzji o dalszym zakresie.
