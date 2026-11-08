# Plan doprowadzenia VGK do pierwszego użytecznego wydania

## Cel

VGK ma być niezależnym systemem operacyjnym dla komputerów x86-64 z procesorami Intel i AMD. Nie może opierać jądra ani docelowego mechanizmu uruchamiania na Linuksie, Windows ani macOS. UEFI i starszy BIOS są interfejsami sprzętowymi, z którymi własny kod VGK musi współpracować.

Ten dokument porządkuje wymagania ustalone dla docelowego systemu. Nie oznacza, że opisane funkcje są już zaimplementowane.

## Zasady prowadzenia prac

1. Zmiany powstają na oddzielnych gałęziach i trafiają do `main` dopiero po przeglądzie oraz świadomym zatwierdzeniu.
2. Nie publikujemy wydania jako gotowego systemu, dopóki nie przejdzie testów uruchamiania, działania i odzyskiwania.
3. Nie usuwamy historycznego prototypu ani kopii źródeł, dopóki nowa ścieżka nie zostanie sprawdzona.
4. Każdy etap otrzymuje jawny status: zaprojektowany, zaimplementowany, sprawdzony kompilacją, sprawdzony w emulatorze, sprawdzony na sprzęcie.
5. Klucz prywatny wydawcy nigdy nie trafia do repozytorium, obrazu instalacyjnego ani artefaktów budowania.
6. Podstawowe zabezpieczenia muszą działać bez połączenia z usługą AI. Inteligentny Opiekun Systemu nie jest jedyną linią obrony.

## Docelowa budowa systemu

### Moduł pierwszy – niezależny mechanizm uruchamiania i ratowania

Ma być oddzielony od właściwego systemu i przechowywany w wydzielonym obszarze nośnika. Jego zadaniem jest:
- rozpoczęcie pracy komputera przez własny kod VGK dla UEFI;
- osobna ścieżka startowa dla starszego BIOS;
- sprawdzenie podpisu wydania i integralności składników przed ich użyciem;
- uruchomienie właściwego systemu i przekazanie mu informacji o sprzęcie;
- udostępnienie minimalnego środowiska odzyskiwania, gdy właściwy system nie może wystartować;
- umożliwienie odbudowy mechanizmu startowego z zaufanego nośnika instalacyjnego.

Moduł pierwszy nie może korzystać z jądra modułu drugiego. Powinien być możliwie mały, ograniczony i domyślnie chroniony przed zapisem. Oddzielna partycja zmniejsza ryzyko uszkodzenia przez zwykłe działanie systemu, ale nie chroni przed awarią całego dysku, firmware ani błędem samego modułu.

### Moduł drugi – właściwy system VGK

Docelowo obejmuje własne jądro, zarządzanie pamięcią i procesami, obsługę urządzeń, system plików, sieć, pulpit, ustawienia, aplikacje i usługi.

Terminal naprawczy nie jest dostępny w zwykłym trybie pracy. Jest dostępny wyłącznie w trybie awaryjnym.

### Inteligentny Opiekun Systemu

Inteligentny Opiekun Systemu ma:
- monitorować procesor, pamięć, dyski, urządzenia, usługi i integralność plików;
- wykrywać istotne zmiany ustawień oraz zapisywać czas i kontekst zdarzenia;
- po wykryciu zmiany wymagającej potwierdzenia pytać użytkownika, czy ją zlecił;
- automatycznie wykonywać wyłącznie wcześniej dozwolone, odwracalne i niskiego ryzyka działania;
- przed poważną zmianą przedstawiać zakres, skutki i sposób wycofania oraz wymagać zgody administratora;
- prowadzić dziennik działań i wyników napraw;
- proponować aktualizacje i ulepszenia, ale nie modyfikować samodzielnie własnych zabezpieczeń ani kluczy zaufania.

Samo pytanie użytkownika nie potwierdza tożsamości osoby przy komputerze. Dlatego system musi też automatycznie blokować nieaktywną sesję i wymagać ponownego uwierzytelnienia. Działania Opiekuna muszą być ograniczone technicznie, a nie tylko opisane w instrukcji.

### Tryb awaryjny

Tryb awaryjny ma trzy ustalone funkcje:

1. **Terminal naprawczy** – dostępny wyłącznie w trybie awaryjnym, z pomocą diagnostyczną. Operacje ryzykowne wymagają zgody administratora i są zapisywane.
2. **Automatyczne migawki systemu** – dwie migawki w miesiącu; przy utworzeniu kolejnej najstarsza jest usuwana, aby zachować ustaloną rotację.
3. **Przywracanie systemu** – odtworzenie systemu z kopii znajdującej się na dysku komputera albo na pendrivie. Przed rozpoczęciem należy sprawdzić czytelność i zgodność kopii, a użytkownik musi potwierdzić operację.

Tworzenie zwykłych kopii zapasowych odbywa się w działającym VGK, za pomocą osobnej aplikacji na pulpicie. Użytkownik wybiera dane i miejsce zapisu, w tym dysk lokalny albo pamięć USB. Aplikacja tworzy i sprawdza kopię, ale nie przywraca systemu. Przywracanie jest dostępne wyłącznie w trybie awaryjnym.

Migawki systemowe nie zastępują kopii zapasowych na innym nośniku i nie chronią przed fizyczną awarią dysku.

## Pulpit i codzienna obsługa

- Nowoczesny, czytelny i konfigurowalny pulpit.
- Pasek z regulowaną długością, grubością, położeniem i wielkością ikon.
- Możliwość zmiany wyglądu, skrótów i elementów pulpitu.
- Centrum powiadomień z ważnymi, a nie spamującymi komunikatami.
- Panel ustawień w układzie dwóch kolumn: kategorie po lewej, szczegółowe ustawienia po prawej.
- Terminal naprawczy niedostępny w zwykłym trybie.

## Bezpieczeństwo

- Zapora sieciowa domyślnie włączona po instalacji.
- Reguły połączeń przychodzących i wychodzących, w tym kontrola adresów IP.
- Osobna kontrola aplikacji i plików: podpis, pochodzenie, integralność i uprawnienia.
- Nieznany program nie otrzymuje automatycznie pełnego zaufania; użytkownik dostaje zrozumiałą informację.
- Zmiany ustawień bezpieczeństwa i plików krytycznych są rejestrowane.
- Błąd weryfikacji podpisu oznacza odmowę uruchomienia chronionego składnika.

## Sprzęt i zgodność

Podstawą jest architektura x86-64. System ma wykrywać możliwości procesora przez CPUID i używać dodatkowych instrukcji tylko wtedy, gdy są dostępne. Wspólna ścieżka podstawowa ma działać na obsługiwanych procesorach Intel i AMD; obsługa nowych generacji, urządzeń i funkcji ma być rozwijana w aktualizacjach. Nie wolno deklarować zgodności z modelem procesora, którego nie przetestowano.

## Pozostałe funkcje użytkowe

- Przeglądarka internetowa dołączana do instalatora po sprawdzeniu licencji, zależności i rozmiaru. Brave jest kandydatem, nie ostateczną decyzją.
- Automatyczne porządkowanie plików tymczasowych i pamięci podręcznej raz na trzy miesiące. Nigdy nie usuwa dokumentów, zdjęć, pobranych plików ani innych danych osobistych.
- Aktualizacje sprawdzane pod kątem zgodności z danym komputerem i podpisu wydawcy.

## Etapy techniczne

### Etap A – wiarygodna podstawa uruchamiania
- Uporządkować i uruchamiać wyłącznie własną ścieżkę UEFI.
- Dokończyć poprawny, ograniczony odczyt plików z nośnika.
- Ustalić i bezpiecznie osadzić potwierdzony klucz publiczny wydawcy.
- Podpisany wykaz ma obejmować rzeczywiste pliki wydania.
- Nie uruchamiać jądra, jeżeli podpis, rozmiar lub skrót pliku są niezgodne.
- Dodać testy z atrapą firmware oraz uruchamianie pod emulatorem UEFI.

### Etap B – własne przekazanie sterowania do jądra
- [x] Zdefiniować wersjonowany protokół informacji startowych VGK.
- [x] Przygotować natywny punkt wejścia x86-64 i minimalne jądro diagnostyczne.
- [x] Dodać walidację zakresów pamięci i normalizację mapy UEFI.
- [x] Zadeklarować ABI usług AllocatePages, GetMemoryMap i ExitBootServices.
- [ ] Podłączyć usługi firmware i rzeczywiście wczytać segmenty ELF.
- Przekazać mapę pamięci, informacje o grafice i tabele sprzętowe udostępnione przez firmware.
- Zakończyć usługi startowe firmware zgodnie z wymaganiami UEFI.
- Załadować i uruchomić własne jądro.
- Nie używać GRUB ani Multiboot2 w nowej ścieżce.

### Etap C – minimalne działające jądro
- Obsługa wyjątków, przerwań i zegara.
- Rozpoznawanie procesora i mapy pamięci.
- Własny allocator pamięci fizycznej i wirtualnej.
- Podstawowa konsola diagnostyczna, a następnie sterowniki.
- Testy w emulatorze i na wybranych komputerach Intel oraz AMD.

### Etap D – podstawowe usługi systemowe
- Procesy, wątki, uprawnienia i izolacja pamięci.
- System plików i bezpieczne operacje na plikach.
- Obsługa klawiatury, myszy, ekranu, USB, dysków i sieci.
- Dziennik zdarzeń i kontrola integralności w czasie pracy.

### Etap E – pierwszy użyteczny pulpit i instalator
- Instalator z możliwością bezpiecznego wyboru miejsca instalacji.
- Konta użytkowników i blokowanie sesji.
- Pulpit, pasek, centrum powiadomień i panel ustawień.
- Zapora domyślnie włączona.
- Przeglądarka i podstawowe aplikacje, w tym aplikacja do tworzenia kopii zapasowych.

### Etap F – odzyskiwanie i Inteligentny Opiekun Systemu
- Niezależny moduł ratunkowy.
- Dwie automatyczne migawki systemu miesięcznie.
- Przywracanie systemu z kopii dostępne wyłącznie w trybie awaryjnym.
- Terminal naprawczy dostępny wyłącznie w trybie awaryjnym.
- Aplikacja tworzenia kopii zapasowych dostępna na zwykłym pulpicie.
- Ograniczony agent lokalny Opiekuna, dziennik i kontrola uprawnień.
- Opcjonalne połączenie z modelem AI; bez wysyłania danych bez zgody użytkownika.

### Etap G – wydanie publiczne
- Testy uruchamiania UEFI oraz osobno BIOS.
- Testy instalacji, aktualizacji, awarii, odtwarzania i bezpieczeństwa.
- Testy na kilku generacjach procesorów Intel i AMD.
- Instrukcja, licencja, suma kontrolna obrazu i podpisane wydanie.
- Publikacja dopiero po przeglądzie i zatwierdzeniu przez administratora projektu.

## Stan rzeczywisty na początku tego planu

Repozytorium zawiera:
- kod punktu wejścia UEFI;
- podstawę odczytu plików z woluminu UEFI;
- kod sprawdzania podpisanego wykazu VGKM i skrótów SHA-256;
- implementację weryfikacji Ed25519 i testy;
- testy układu struktur UEFI oraz testy integracyjne z atrapą protokołu plików.

Ograniczenia:
- nie ma jeszcze potwierdzonego, produkcyjnego klucza publicznego wydawcy;
- nie ma kompletnego ładowania i uruchamiania jądra przez nową ścieżkę;
- brak kompletnego własnego jądra, sterowników, systemu plików, instalatora i pulpitu;
- własna ścieżka starszego BIOS nie jest gotowa;
- testy struktury i testy z atrapą firmware nie zastępują testu w emulatorze UEFI ani na fizycznym komputerze.

Historyczny prototyp 0.1 i jego pliki należy zachować jako materiał archiwalny do czasu potwierdzenia nowej ścieżki. Nie należy przywracać GRUB/Multiboot2 do docelowego mechanizmu VGK.


## Dodatkowe aplikacje użytkowe – nośnik instalacyjny i biuro

### Aplikacja do przygotowania pendrive'a VGK
- Prosty kreator podobny w obsłudze do Rufusa: wybór obrazu VGK, wybór pendrive'a, kontrola obrazu, zapis i sprawdzenie wyniku.
- Weryfikacja podpisu wydania oraz sumy kontrolnej przed zapisem.
- Wyraźne ostrzeżenie o usunięciu danych z wybranego pendrive'a i obowiązkowe potwierdzenie.
- Tworzenie nośnika zgodnego z własnym mechanizmem uruchamiania VGK; obsługa UEFI jako pierwsza, starszy BIOS dopiero po ukończeniu osobnej ścieżki.
- Aplikacja nie może po cichu modyfikować dysku systemowego ani innych nośników.

### Instalator systemu
To osobny program uruchamiany z nośnika instalacyjnego. Prowadzi przez wybór dysku, sposób instalacji, ustawienia użytkownika i końcową kontrolę. Musi jasno pokazywać, które partycje i dane zostaną zmienione. Instalacja obok innego systemu będzie dostępna dopiero po bezpiecznym przygotowaniu i przetestowaniu obsługi partycji.

### Pakiet biurowy
Docelowy zestaw obejmuje edytor tekstu, arkusz kalkulacyjny i program prezentacyjny. Preferowana jest obsługa OpenDocument oraz praktyczna zgodność z dokumentami Microsoft Office. Przed wyborem gotowego pakietu należy sprawdzić licencję, wymagania pamięciowe i sprzętowe, rozmiar oraz możliwość bezpiecznego uruchomienia w VGK. Programy biurowe są aplikacjami użytkowymi – nie zmieniają niezależnej budowy jądra VGK.
