# Tryb awaryjny VGK — projekt techniczny

## Status dokumentu

**Status: zaprojektowany, niezaimplementowany.** Ten dokument opisuje wymagania i granice bezpieczeństwa. Nie oznacza, że terminal, środowisko ratunkowe, migawki, przywracanie ani pomoc internetowa już działają.

Tryb awaryjny jest odrębnym, małym środowiskiem VGK. Ma uruchamiać się niezależnie od zwykłego systemu, aby awaria pulpitu, usług lub plików systemowych nie uniemożliwiała diagnostyki i odzyskiwania.

## 1. Zasada: trzy funkcje, dwa źródła pomocy

Tryb awaryjny zachowuje dokładnie trzy funkcje ustalone dla VGK:

1. terminal naprawczy;
2. automatyczne migawki systemu;
3. przywracanie systemu z kopii lokalnej lub nośnika USB.

Pomoc lokalna i pomoc internetowa nie są dodatkowymi funkcjami systemu — są dwoma sposobami uzyskiwania pomocy w ramach terminala naprawczego.

### Pomoc lokalna

- Działa bez sieci i nie wymaga modelu sztucznej inteligencji.
- Wykonuje sprawdzone kontrole: integralność plików, stan systemu plików, podstawowe informacje o pamięci i urządzeniach oraz analizę dzienników.
- Rozpoznane, bezpieczne czynności może proponować jako gotowe działania.
- Nieznana lub ryzykowna naprawa nie jest wykonywana automatycznie.
- Wyniki diagnostyki i podjęte działania zapisuje w lokalnym dzienniku.

### Pomoc internetowa

- Jest opcjonalna i uruchamiana wyłącznie na żądanie użytkownika.
- Przed wysłaniem danych pokazuje, co zostanie przekazane; domyślnie usuwa hasła, klucze, tokeny i prywatne treści.
- Wysyła tylko niezbędne informacje diagnostyczne, po wyraźnej zgodzie użytkownika.
- Może analizować raport, objaśnić przyczynę awarii i przygotować propozycję naprawy.
- Nie otrzymuje zdalnego terminala, nie wykonuje poleceń na komputerze i nie może samodzielnie zmieniać plików.
- Każda poprawka musi zostać pobrana z zaufanego źródła, sprawdzona pod względem podpisu i integralności, a jej zakres i skutki muszą być pokazane użytkownikowi.
- Istotna naprawa wymaga zatwierdzenia przez użytkownika na komputerze. Brak internetu nie może blokować napraw lokalnych ani przywracania.

## 2. Własny terminal naprawczy

Terminal będzie rzeczywistym, tekstowym interfejsem VGK, a nie obrazem ani nakładką graficzną. Ma być prosty, czytelny i obsługiwany klawiaturą. Komunikaty i podstawowe polecenia będą po polsku.

Wymagane zachowanie:

- uruchamia się wyłącznie w odseparowanym trybie awaryjnym;
- nie jest dostępny w zwykłym pulpicie ani dla zwykłych aplikacji;
- pokazuje wykryty problem, poziom ryzyka i proponowane działanie;
- rozróżnia polecenia tylko do odczytu od poleceń zmieniających dane;
- przed operacją mogącą zmienić system pokazuje dokładny zakres i wymaga potwierdzenia;
- nie przyjmuje z sieci poleceń do bezpośredniego wykonania;
- zapisuje datę, wynik, zakres i zgodę dla każdej naprawy;
- przy braku klawiatury lub ekranu nadal może udostępnić ograniczoną diagnostykę przez obsługiwany kanał serwisowy, jeśli sprzęt go zapewnia.

Terminal nie może być jedynym sposobem naprawy. Przywracanie z kopii ma mieć osobną, prostą i czytelną procedurę w tym samym środowisku awaryjnym.

## 3. Oddzielenie i ochrona środowiska ratunkowego

- Środowisko ratunkowe ma własne pliki, minimalne usługi i tylko niezbędne sterowniki.
- Jego składniki są sprawdzane przed uruchomieniem na podstawie zaufanego, podpisanego wykazu.
- Zwykły system nie może po cichu modyfikować plików środowiska ratunkowego.
- Środowisko ratunkowe nie ufa kopii systemu tylko dlatego, że znajduje się na lokalnym dysku lub USB.
- Jeśli pliki ratunkowe są uszkodzone albo ich podpis jest nieprawidłowy, VGK ma odmówić ich użycia i wskazać uruchomienie z zaufanego nośnika instalacyjnego/ratunkowego.
- Oddzielny obszar na tym samym dysku nie chroni przed fizyczną awarią całego dysku. Dlatego użytkownik musi móc użyć zewnętrznego nośnika USB.

## 4. Automatyczne migawki systemu

Wymaganie użytkowe pozostaje następujące: system tworzy dwie automatyczne migawki w miesiącu. Przy utworzeniu kolejnej najstarsza migawka jest usuwana zgodnie z ustaloną rotacją.

Wymagania bezpieczeństwa:

- migawka musi mieć spójny punkt odniesienia, wersję systemu i wykaz składników;
- jej zawartość musi być sprawdzana pod względem czytelności i integralności;
- nie wolno usuwać poprzedniej poprawnej migawki, zanim nowa nie zostanie w całości utworzona i zweryfikowana;
- jeśli zabraknie miejsca lub weryfikacja się nie powiedzie, zachować istniejące poprawne migawki i zgłosić problem;
- rotacja nie może usuwać dokumentów ani prywatnych danych użytkownika;
- migawki systemowe nie zastępują kopii danych na innym nośniku i nie chronią przed awarią całego dysku.

Sposób techniczny migawkowania zależy od przyszłego systemu plików VGK. Nie należy deklarować tej funkcji jako gotowej, dopóki nie powstanie i nie przejdzie testów.

## 5. Przywracanie systemu

Przywracanie jest dostępne wyłącznie w trybie awaryjnym. Źródłem może być lokalna kopia albo pendrive.

Przed rozpoczęciem VGK musi:

1. rozpoznać wybraną kopię i pokazać jej wersję oraz datę;
2. sprawdzić podpis, wykaz plików, skróty i kompletność kopii;
3. sprawdzić, czy kopia pasuje do architektury, układu i wersji systemu;
4. sprawdzić, czy docelowy dysk i wymagane miejsce są dostępne;
5. pokazać użytkownikowi, co zostanie zastąpione;
6. uzyskać wyraźne potwierdzenie;
7. wykonać operację w sposób umożliwiający wykrycie przerwania i bezpieczne wznowienie albo wycofanie, o ile pozwala na to nośnik i stan sprzętu.

Nie wolno przywracać kopii z nieprawidłowym podpisem, uszkodzonym wykazem lub niezgodnymi skrótami. Nie wolno automatycznie formatować całego dysku ani zmieniać partycji bez osobnego, jednoznacznego potwierdzenia.

Jeśli to możliwe, przed nadpisaniem aktualnego systemu należy zachować jego stan lub utworzyć punkt wycofania. Jeżeli nie ma wystarczającego miejsca albo bezpieczna operacja nie jest możliwa, VGK ma przerwać przed zmianą danych i wyjaśnić przyczynę.

## 6. Bezpieczny zakres napraw

Naprawy dzielą się na trzy poziomy:

- **Odczytowe:** diagnostyka i raportowanie; mogą działać bez potwierdzania każdej czynności.
- **Ograniczone i odwracalne:** naprawa znanego pliku z zaufanej kopii, jeśli można odtworzyć poprzedni stan; wymagają pokazania zakresu i zgody zgodnie z polityką systemu.
- **Wysokiego ryzyka:** zmiany partycji, formatowanie, nadpisanie systemu, zmiany rozruchu lub operacje bez możliwości wycofania; wymagają osobnego, jednoznacznego potwierdzenia i dodatkowej kontroli.

Nieznane polecenie, nieznana poprawka lub niezweryfikowany plik nie mogą być uruchamiane. Pomoc internetowa może doradzać, ale ostateczne sprawdzenie i wykonanie odbywa się lokalnie w VGK.

## 7. Kolejność realizacji i testy

1. Zaprojektować minimalny start odseparowanego środowiska ratunkowego.
2. Zapewnić sprawdzanie podpisu i integralności jego plików przed użyciem.
3. Przygotować lokalny moduł diagnostyczny i dziennik.
4. Zbudować tekstowy terminal naprawczy dostępny wyłącznie w trybie awaryjnym.
5. Opracować format migawki i bezpieczną rotację dwóch kopii miesięcznie.
6. Zbudować weryfikację kopii i procedurę przywracania.
7. Dopiero później dodać opcjonalne połączenie internetowe z minimalizacją danych i bez zdalnego wykonywania poleceń.
8. Testować awarie zasilania, uszkodzone kopie, nieprawidłowe podpisy, brak sieci, brak miejsca, błędy dysku i przerwanie przywracania.

Testy muszą obejmować zarówno poprawne przypadki, jak i próby podmiany plików, nieautoryzowane operacje, niezgodność kopii oraz przerwanie procesu. Kompilacja lub test jednostkowy nie zastępuje próby w emulatorze i na fizycznym sprzęcie.

## 8. Powiązane wymagania

Ogólne wymagania użytkowe i zasada trzech funkcji znajdują się w pliku docs/WYMAGANIA_UZYTKOWE_VGK.md. Plan etapów całego systemu znajduje się w pliku docs/PLAN_WDROZENIA.md.

**Stan na dziś:** dokumentacja trybu awaryjnego jest projektem. W repozytorium nie ma jeszcze działającego terminala ratunkowego, migawek ani przywracania systemu.


## 0. Dwa moduły systemowe i dwa warianty uruchamiania komputera

W VGK trzeba rozróżniać dwa niezależne podziały. Nie są one tym samym:

### Dwa moduły systemowe

**Moduł pierwszy – Środowisko Startowe i Ratunkowe** działa niezależnie od właściwego systemu. Odpowiada za rozpoczęcie uruchamiania VGK, kontrolę zaufania i integralności, uruchomienie ekranu logowania oraz udostępnienie trybu awaryjnego. To on ma pozostać dostępny, gdy moduł drugi ulegnie awarii. Musi być możliwie mały, odseparowany i chroniony przed zmianami przez zwykły system.

**Moduł drugi – System Właściwy** przejmuje pracę po pomyślnym zalogowaniu użytkownika. Obejmuje jądro i usługi zwykłego systemu, pulpit, sterowniki, aplikacje oraz codzienne funkcje. Jego awaria nie może uszkodzić ani zablokować modułu pierwszego.

Moduł pierwszy nie może zależeć od jądra ani usług modułu drugiego. Instalator VGK ma zawierać funkcję **„Odbuduj mechanizm startowy pierwszego modułu”**, która z zaufanego nośnika sprawdzi i odtworzy wyłącznie składniki modułu pierwszego. Operacja musi pokazać zakres zmian i wymagać potwierdzenia. Nie może przy tym kasować danych użytkownika ani bez potrzeby reinstalować modułu drugiego.

Ekran logowania należy do modułu pierwszego. Po poprawnym uwierzytelnieniu następuje kontrolowane przekazanie działania do modułu drugiego. Szczegóły tego przekazania, izolacji pamięci i obsługi sesji wymagają osobnego projektu technicznego.

### Dwa warianty startu sprzętowego

Moduł pierwszy ma mieć dwa własne warianty dostosowane do oprogramowania startowego płyty głównej:

- **VGK UEFI** – dla komputerów uruchamianych w trybie UEFI;
- **VGK Legacy** – dla komputerów ze starszym BIOS-em.

To dwa warianty uruchomienia tego samego modułu pierwszego, a nie dwa różne systemy operacyjne ani dwa jądra. Docelowy nośnik instalacyjny ma zawierać oba warianty i automatycznie wybrać właściwy dla trybu uruchomienia komputera. Oba mają prowadzić do tego samego środowiska startowego VGK i dalej do tego samego Systemu Właściwego. Wariant Legacy zostanie opracowany po bezpiecznym uruchomieniu i przetestowaniu ścieżki UEFI.

**Zasada nadrzędna:** problem lub uszkodzenie Systemu Właściwego nie może odebrać użytkownikowi dostępu do ekranu logowania, trybu awaryjnego ani procedury odbudowy modułu pierwszego.
