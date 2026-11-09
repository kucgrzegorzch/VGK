# Plan obsługi wyjątków procesora VGK

## Cel

Dodać kontrolowaną obsługę wyjątków x86-64, zanim VGK zacznie włączać przerwania sprzętowe lub uruchamiać wiele zadań. Błąd procesora ma zostać zapisany i przedstawiony w diagnostyce, a nie prowadzić do przypadkowego wykonania nieznanego kodu.

Ten dokument opisuje kolejność wdrożenia. Sam w sobie nie oznacza, że obsługa wyjątków jest już aktywna.

## Zasady bezpieczeństwa

- Na początku wszystkie wyjątki pozostają obsługiwane na poziomie jądra (CPL 0); nie udostępniamy bramek użytkownikowi.
- Procedury wejściowe muszą zachować wszystkie rejestry, których używa kod C, oraz spełnić wymagania ABI x86-64.
- Nie wolno zakładać jednego układu stosu dla wszystkich wyjątków: część wyjątków procesora sama umieszcza kod błędu, a część nie.
- Wspólny punkt wejścia otrzymuje jawnie znormalizowany zapis: numer wektora, informację o obecności kodu błędu, kod błędu (albo zero) i zapis rejestrów.
- Procedura awaryjna nie może korzystać z alokatora pamięci, systemu plików, sieci ani blokad, które mogły zostać uszkodzone przez sam wyjątek.
- W przypadku wyjątku krytycznego zapisujemy możliwie mało danych do wcześniej przygotowanego bufora, wysyłamy komunikat diagnostyczny i zatrzymujemy procesor.
- Nie próbujemy automatycznie „naprawiać” nieznanego wyjątku. Automatyczna naprawa może być dodana dopiero dla konkretnych, rozpoznanych i odwracalnych przypadków.

## Wektory wyjątków

Pierwsza wersja przygotowuje wpisy dla wektorów 0–31. Procedury asemblerowe muszą jawnie rozróżniać wyjątki, dla których procesor odkłada kod błędu.

| Wektor | Wyjątek | Kod błędu procesora |
|---:|---|---|
| 0 | Dzielenie przez zero (#DE) | nie |
| 1 | Pojedynczy krok (#DB) | nie |
| 2 | NMI | nie |
| 3 | Pułapka debugowania (#BP) | nie |
| 4 | Przepełnienie (#OF) | nie |
| 5 | Granice (#BR) | nie |
| 6 | Nieprawidłowa instrukcja (#UD) | nie |
| 7 | Brak urządzenia zmiennoprzecinkowego (#NM) | nie |
| 8 | Podwójny błąd (#DF) | tak, kod 0 |
| 9 | Zarezerwowany (historycznie coprocessor segment overrun) | nie |
| 10 | Nieprawidłowy segment TSS (#TS) | tak |
| 11 | Segment nieobecny (#NP) | tak |
| 12 | Błąd stosu (#SS) | tak |
| 13 | Błąd ochrony ogólnej (#GP) | tak |
| 14 | Błąd strony (#PF) | tak |
| 15 | Zarezerwowany | nie |
| 16 | Błąd x87 (#MF) | nie |
| 17 | Sprawdzenie wyrównania (#AC) | tak |
| 18 | Błąd kontroli maszyny (#MC) | nie |
| 19 | Błąd SIMD (#XM/#XF) | nie |
| 20 | Wirtualizacja (#VE) | tak |
| 21 | Ochrona kontrolna (#CP) | tak |
| 22–27 | Zarezerwowane / zależne od rozszerzeń | nie w bazowym zestawie |
| 28 | Hypervisor Injection Exception (#HV) | nie |
| 29 | VMM Communication Exception (#VC) | tak |
| 30 | Security Exception (#SX) | tak |
| 31 | Zarezerwowany | nie |

W praktycznym kodzie lista musi być zgodna z architekturą procesora i sprawdzona z dokumentacją Intel/AMD. Wektory zarezerwowane pozostają bezpiecznie obsłużone jako nieznane; nie zakładamy, że każdy procesor implementuje rozszerzenia z wektorów 20–30.

## Kolejność wdrożenia

### Etap 1 — wspólny zapis diagnostyczny

- Zdefiniować strukturę zapisu wyjątku z numerem wektora, kodem błędu, wskaźnikiem instrukcji, rejestrem CS/RFLAGS, wskaźnikiem stosu oraz zachowanymi rejestrami ogólnego przeznaczenia.
- Zdefiniować stałe identyfikujące wyjątki krytyczne i te, które mogą mieć znaczenie diagnostyczne.
- Dodać testy rozmiaru, wyrównania, przesunięć pól i klasyfikacji wektorów.
- Nie odczytywać adresu CR2 z poziomu zwykłego kodu C. Dla #PF asemblerowy punkt wejścia ma odczytać CR2 natychmiast i umieścić go w zapisie diagnostycznym.

### Etap 2 — procedury wejściowe

- Utworzyć osobną procedurę wejściową dla każdego wektora 0–31 albo bezpiecznie generowane, jednoznaczne procedury pośrednie.
- Każda procedura bez kodu błędu sama umieszcza wartość zastępczą; procedura wyjątku z kodem błędu zachowuje kod dostarczony przez procesor.
- Wspólny fragment zapisuje rejestry, ustala wyrównanie stosu i wywołuje funkcję C.
- Nie wykonywać instrukcji STI; wyjątki nie mogą przypadkowo uruchomić przerwań maskowalnych.
- Sprawdzić zgodność kolejności odkładania danych z układem struktury C testem asemblerowym i testem QEMU.

### Etap 3 — TSS i stosy awaryjne

- Przed użyciem pól IST przygotować deskryptor TSS 64-bitowego, załadować TR i przydzielić osobne, wyrównane stosy.
- Stos IST 1 przeznaczyć dla podwójnego błędu (#DF); osobne stosy dla NMI i błędu kontroli maszyny rozważyć po testach.
- Każdy stos ma mieć ustalony rozmiar, granice i ochronę przed nakładaniem.
- Do czasu poprawnej inicjalizacji TSS nie wolno instalować wpisów IDT, które wskazują na nieprzygotowane IST.
- Testować celowo wywołane wyjątki w QEMU/OVMF; podwójny błąd wymaga osobnego testu i nie może być pierwszym testem na fizycznym komputerze.

### Etap 4 — instalacja IDT

- Zbudować kompletną tablicę w pamięci jądra i sprawdzić wszystkie wpisy przed instalacją.
- Sprawdzić selektor segmentu kodu, kanoniczność adresów procedur, typ bramki, DPL=0, present=1 i prawidłowe numery IST.
- Wykonać LIDT dopiero po gotowości kodu wejściowego, stosów i TSS.
- Początkowo pozostawić przerwania maskowalne wyłączone (IF=0).
- Dodać test, że brak lub błędny wpis nie powoduje instalacji częściowej tablicy.

### Etap 5 — diagnostyka

- Dla #PF zapisać CR2, kod błędu, RIP i podstawowe informacje o mapowaniu pamięci, o ile ich odczyt nie wymaga ryzykownej alokacji.
- Dla #GP/#SS/#TS/#NP zapisać kod błędu i selektory, jeśli są dostępne.
- Dla pozostałych wyjątków zapisać wektor i kontekst rejestrów.
- Komunikat ma podawać numer i nazwę wyjątku oraz informację, czy system został zatrzymany.
- Bufor diagnostyczny jest stały i ograniczony; nie może nadpisywać pamięci poza swoim zakresem.

## Wymagane testy przed aktywacją

1. Testy jednostkowe układu zapisu wyjątku i mapowania wektorów.
2. Test kompilacji wszystkich procedur asemblerowych i sprawdzenie symboli/linkowania.
3. Test kontrolowanego #UD i #DE w QEMU/OVMF.
4. Test #PF z oczekiwanym adresem CR2 i kodem błędu.
5. Test #GP oraz weryfikacja, że zapis diagnostyczny jest spójny.
6. Test podwójnego błędu na osobnym stosie IST.
7. Test braku przypadkowego STI i test pozostawienia przerwań maskowalnych wyłączonych.
8. Test negatywny: błędny selektor, adres niekanoniczny, niegotowy TSS lub nieprawidłowy stos mają zatrzymać inicjalizację przed LIDT.

## Czego ten etap nie uruchamia

- Nie włącza jeszcze przerwań sprzętowych ani kontrolera APIC/PIC.
- Nie uruchamia planisty, wielozadaniowości ani przełączania stosów zadań.
- Nie tworzy jeszcze mechanizmu naprawy pamięci lub procesu po wyjątku.
- Nie oznacza obsługi wyjątków na fizycznym sprzęcie; do tego potrzebny będzie osobny, ostrożny test rozruchu.

## Kryterium zakończenia

Etap można uznać za zakończony dopiero wtedy, gdy procedury wejściowe, TSS/IST, instalacja IDT, diagnostyka i testy kontrolowanych wyjątków przejdą w CI oraz QEMU/OVMF. Dopiero potem można rozpocząć osobny etap przerwań sprzętowych.


## Szczegoly ukladu stosu i kontraktu asembler–C

Przed napisaniem wspolnego punktu wejscia trzeba zachowac nastepujace rozroznienia:

1. Dla wyjatku bez kodu bledu procesor zapisuje RIP, CS i RFLAGS. Procedura wektorowa doklada wartosc zastępcza kodu bledu (zero) oraz numer wektora.
2. Dla wyjatku z kodem bledu procesor zapisuje kod bledu przed RIP, CS i RFLAGS. Procedura wektorowa doklada tylko numer wektora.
3. Po tych operacjach poczatek znormalizowanej ramki jest taki sam: numer wektora, kod bledu, RIP, CS, RFLAGS.
4. Nie wolno jednak zakladac, ze po RFLAGS zawsze znajduja sie zapisane przez procesor stare RSP i SS. Przy zwyklym wejsciu na ten sam poziom uprawnien bez IST procesor ich nie doklada. Przy zmianie stosu przez IST lub zmiane poziomu uprawnien zapisuje takze poprzednie RSP i SS.

W konsekwencji wspolny zapis diagnostyczny musi otrzymywac jawna informacje o rodzaju ramki (czy procesor zmienil stos). Dla ramki bez zmiany stosu poprzedni RSP trzeba wyliczyc z adresu ramki, a SS pobrac jako aktualny selektor. Dla ramki ze zmiana stosu poprzednie RSP i SS pochodza z ramki sprzetowej. Nie wolno odczytywac tych pol wedlug jednego stalego przesuniecia.

### Kolejnosc przygotowania wspolnego wejscia

- Zachowac wszystkie rejestry ogolnego przeznaczenia przed uzyciem ich jako rejestrow roboczych.
- Odczytac CR2 na samym poczatku wspolnej sciezki dla #PF; dla innych wektorow pole CR2 oznaczyc jako nieistotne albo zapisac zero.
- Przekazac do kodu C wskaznik do jawnie zbudowanej struktury, a nie rzutowac bezposrednio surowego stosu CPU na VGK_EXCEPTION_CONTEXT.
- Ustalic wyrownanie stosu zgodne z ABI przed wywolaniem C. Nie zakladac, ze stos przerwany w dowolnym miejscu ma wymagane wyrownanie.
- Po zapisaniu kontekstu nie wracac z wyjatku w pierwszej wersji. Funkcja diagnostyczna ma zakonczyc sie kontrolowanym zatrzymaniem.

### Warunki blokujace instalacje IDT

Nie wolno wykonac LIDT, dopoki:
- wszystkie 32 procedury maja sprawdzony kontrakt stosu;
- wspolny punkt wejscia zachowuje rejestry i poprawnie wyrownuje stos;
- GDT, TSS i stosy IST sa gotowe;
- dla kazdego wektora znany jest typ ramki oraz numer IST;
- testy QEMU potwierdzaja poprawny zapis kontekstu dla #UD, #DE, #PF i osobno dla wejscia przez IST.

Obecne procedury w `boot/exception_stubs.S` nadal tylko przekazuja wektor i zatrzymuja procesor. Powyzszy opis jest specyfikacja nastepnego etapu, a nie deklaracja, ze pelny zapis rejestrow juz dziala.
