# Model zagrożeń VGK

**Status:** propozycja architektoniczna. Dokument określa zagrożenia, granice zaufania i wymagane reakcje. Nie oznacza, że opisane mechanizmy są już zaimplementowane lub aktywne.

**Zakres:** docelowy, niezależny system VGK dla komputerów x86-64. Model obejmuje uruchamianie, jądro, pamięć, sterowniki, aplikacje, pliki, aktualizacje, sieć i odzyskiwanie.

## 1. Cel i sposób użycia modelu

Model ma być podstawą projektowania VGK Guardian Security oraz Systemu Naprawczego. Każda nowa funkcja związana z bezpieczeństwem powinna zostać oceniona względem tego dokumentu przed implementacją.

Podstawowe cele:
- zapobiegać uruchamianiu nieautoryzowanych lub zmodyfikowanych składników;
- ograniczać skutki przejęcia zwykłej aplikacji;
- chronić jądro, pamięć jądra, dane użytkownika i klucze zaufania;
- wykrywać naruszenia i przedstawiać użytkownikowi zrozumiałe informacje;
- odzyskiwać sprawność systemu bez niepotrzebnego ryzyka utraty danych;
- działać lokalnie w podstawowym zakresie, bez wymogu płatnej usługi lub stałego Internetu.

Model należy aktualizować, gdy zmienia się architektura, pojawiają się nowe interfejsy uprzywilejowane albo testy ujawniają nieuwzględnione zagrożenie.

## 2. Zasoby wymagające ochrony

| Zasób | Co należy chronić |
|---|---|
| Łańcuch uruchamiania | firmware/UEFI, program startowy, wykaz VGKM, jądro i przekazywane informacje startowe |
| Klucze zaufania | klucz publiczny wydawcy, procedura zmiany klucza; klucz prywatny wydawcy musi pozostać poza systemem i repozytorium |
| Jądro | kod, dane, tablice stron, struktury zarządzania pamięcią i uprzywilejowany stan CPU |
| Pamięć | izolacja przestrzeni adresowych, ochrona stron, stosy i bufory, dane procesów |
| Sterowniki | pochodzenie, integralność, uprawnienia, dostęp do urządzeń i pamięci |
| Aplikacje i procesy | tożsamość, uprawnienia, izolacja, dane i możliwość wykonywania kodu |
| Pliki systemowe | pliki wykonywalne, biblioteki, konfiguracja, komponenty odzyskiwania |
| Dane użytkownika | poufność, integralność, dostępność i możliwość odtworzenia |
| Aktualizacje | autentyczność, integralność, wersja, kolejność instalacji i możliwość odzyskania |
| Nośniki wymienne | pliki, systemy plików, obrazy startowe i urządzenia USB |
| Sieć | ruch przychodzący i wychodzący, konfiguracja i dane uwierzytelniające |
| Dzienniki | wiarygodność zdarzeń, ochrona przed cichym usunięciem i ograniczenie danych osobowych |

## 3. Granice zaufania

1. **Firmware i UEFI** – zewnętrzna podstawa startu. VGK nie może zakładać, że firmware jest wolne od błędów lub kompromitacji. Dostępność i jakość weryfikacji startu zależą od sprzętu.
2. **Program uruchamiający VGK** – ma sprawdzać podpis wykazu i integralność składników przed ich użyciem. Dopóki sprawdzanie nie jest rzeczywiście wywoływane w ścieżce startowej z potwierdzonym kluczem, nie stanowi aktywnej granicy ochronnej.
3. **Jądro VGK** – docelowy autorytet dla operacji uprzywilejowanych, izolacji i ochrony pamięci. Nie powinno ufać żądaniom aplikacji wyłącznie na podstawie ich nazwy.
4. **Administrator** – może wykonywać czynności administracyjne, ale nie powinien móc przypadkowo ani bez śladu wyłączyć ochrony krytycznej. Zmiany zaufania wymagają osobnej, jawnej procedury.
5. **Zwykłe aplikacje** – niezaufane z definicji. Otrzymują minimalne uprawnienia i nie mają bezpośredniego dostępu do pamięci jądra ani kluczy zaufania.
6. **Sterowniki** – składniki wysokiego ryzyka ze względu na uprzywilejowany dostęp. Wymagają silniejszej weryfikacji niż zwykłe aplikacje.
7. **USB i inne nośniki** – źródła danych niezaufane do czasu sprawdzenia. Sam fakt podłączenia urządzenia nie może powodować automatycznego uruchomienia zawartego na nim kodu.
8. **Sieć i pobrane pliki** – źródła niezaufane. Szyfrowane połączenie nie jest samo w sobie dowodem bezpieczeństwa pobranego programu.
9. **Zewnętrzne skanery i narzędzia ochronne** – mogą być użyteczne, ale nie otrzymują automatycznie uprawnień jądra. Docelowo działają przez ograniczone, wersjonowane API VGK.

## 4. Przeciwnicy i scenariusze

| Scenariusz | Przykładowe działanie | Priorytet ochrony |
|---|---|---|
| Złośliwa aplikacja użytkownika | próbuje odczytać dane innego procesu, uzyskać uprawnienia lub zmienić pliki systemowe | izolacja, minimalne uprawnienia, kontrola dostępu |
| Przejęty proces | legalna aplikacja zostaje wykorzystana do uruchomienia nieautoryzowanych działań | ograniczenie uprawnień procesu, kontrola operacji uprzywilejowanych |
| Złośliwy lub zmodyfikowany sterownik | próbuje zapisywać pamięć jądra, omijać ochronę lub przejąć urządzenie | weryfikacja przed załadowaniem, ograniczony interfejs sterownika, możliwość odmowy |
| Zainfekowany nośnik USB | dostarcza złośliwy plik, spreparowany system plików lub obraz startowy | brak automatycznego uruchamiania, bezpieczny odczyt, skanowanie i kontrola integralności |
| Podmieniona aktualizacja | dostarcza zmieniony pakiet lub starszą podatną wersję | podpis, kontrola wersji, atomowe wdrożenie i odzyskiwanie |
| Atak sieciowy | próbuje wykorzystać usługę, zmienić pobierany plik lub przejąć konto | ograniczenie usług, walidacja danych, kontrola sieci i aktualizacji |
| Atakujący z dostępem fizycznym | modyfikuje dysk, podmienia nośnik lub próbuje uruchomić własny system | weryfikacja startu, szyfrowanie danych jako osobny przyszły wymóg, jawne ograniczenia |
| Kompromitacja firmware lub klucza wydawcy | zmienia zaufany początek łańcucha albo podpisuje złośliwe wydanie | procedura unieważnienia i rotacji kluczy; ograniczenie skutków, nie obietnica pełnej ochrony |

Model nie zakłada, że każdy atakujący ma te same możliwości. Zwykła aplikacja nie powinna móc osiągnąć skutków ataku z uprawnieniami jądra. Jednocześnie kompromitacja firmware, sprzętu lub klucza wydawcy może przekroczyć możliwości ochronne samego systemu operacyjnego.

## 5. Zasady obrony

### 5.1 Domyślna odmowa

- Nieznany lub niezweryfikowany składnik nie otrzymuje uprawnień uprzywilejowanych.
- Błąd weryfikacji podpisu albo integralności oznacza odmowę użycia danego składnika.
- Brak Internetu nie może powodować automatycznego wyłączenia lokalnych kontroli.
- Brak danych potrzebnych do bezpiecznej decyzji powinien prowadzić do zachowawczej odmowy, a nie do cichego pominięcia kontroli.

### 5.2 Minimalne uprawnienia i izolacja

- Aplikacja otrzymuje tylko uprawnienia niezbędne do zadeklarowanej funkcji.
- Dostęp do plików, urządzeń, sieci i procesów powinien być przyznawany w możliwie wąskim zakresie.
- Sterownik nie może otrzymywać nieograniczonego dostępu tylko dlatego, że jest wymagany przez urządzenie.
- Operacje uprzywilejowane przechodzą przez kontrolowane interfejsy jądra.

### 5.3 Integralność i zaufanie

- Skrót pliku wykrywa zmianę zawartości, ale nie potwierdza autora.
- Zaufanie do wydania wymaga podpisu wykazu przez zaufany klucz publiczny.
- Podpis musi być sprawdzony przed interpretacją wykazu i użyciem plików.
- Należy uwzględnić cofnięcie do starej, poprawnie podpisanej, lecz podatnej wersji.
- Klucz prywatny wydawcy nie może być umieszczany w repozytorium, ISO, artefaktach CI ani w systemie użytkownika.

### 5.4 Audyt i prywatność

- Istotne decyzje bezpieczeństwa powinny mieć zapis: co się stało, którego zasobu dotyczyło, jaka reguła zadziałała i jaki był wynik.
- Dzienniki nie powinny bez potrzeby przechowywać treści prywatnych plików, haseł ani pełnych danych osobowych.
- Aplikacja nie może samodzielnie usuwać lub fałszować zaufanych zapisów zdarzeń.
- Użytkownik powinien otrzymać komunikat zrozumiały, a nie wyłącznie kod błędu.

## 6. Automatyczne reakcje i granice samodzielnej naprawy

### 6.1 Działania, które docelowo mogą być automatyczne

Pod warunkiem, że mechanizm został zaimplementowany i przetestowany:
- odmowa uruchomienia pliku, którego podpis lub integralność są nieprawidłowe;
- odmowa załadowania nieautoryzowanego sterownika;
- zatrzymanie operacji uprzywilejowanej, której uprawnienia nie zostały potwierdzone;
- ograniczenie lub odizolowanie procesu w sposób odwracalny;
- tymczasowe zablokowanie połączenia sieciowego, które narusza ustaloną regułę;
- zapisanie zdarzenia i przedstawienie użytkownikowi informacji;
- przejście do trybu ograniczonego, jeżeli można to zrobić bezpiecznie.

Automatyczna blokada nie może sama w sobie oznaczać usunięcia pliku użytkownika.

### 6.2 Działania wymagające zgody lub szczególnej procedury

- trwałe usunięcie pliku użytkownika lub całego katalogu;
- formatowanie, naprawa systemu plików lub przebudowa partycji;
- przywracanie kopii, jeżeli może nadpisać nowsze dane;
- cofnięcie aktualizacji, które może zmienić dane użytkownika;
- zmiana lub unieważnienie klucza zaufania;
- trwałe usunięcie sterownika lub aplikacji, od których zależy sprzęt albo dostęp do danych.

W sytuacji awaryjnej system może odizolować zagrożony element, ale powinien zachować dane do analizy, o ile nie zwiększa to ryzyka. Gdy nie da się zagwarantować bezpiecznej naprawy, VGK powinien zatrzymać operację i wyjaśnić użytkownikowi, czego nie może bezpiecznie wykonać.

### 6.3 Zasada naprawy

1. Rozpoznać problem i zebrać minimalne dane diagnostyczne.
2. Zabezpieczyć możliwość odzyskania danych lub zachować kopię stanu.
3. Zweryfikować źródło pliku naprawczego.
4. Wykonać najmniejszą możliwą zmianę.
5. Ponownie sprawdzić wynik.
6. Jeśli kontrola się nie powiedzie, przerwać i przejść do bezpiecznego trybu odzyskiwania.

System Naprawczy nie może „naprawiać” problemu przez wyłączanie zabezpieczeń ani instalowanie niezweryfikowanych plików.

## 7. Zewnętrzne programy ochronne

VGK powinien zapewniać własną podstawową ochronę, ale nie blokować bez uzasadnienia wszystkich dodatkowych skanerów.

Docelowa polityka:
- skaner działający w przestrzeni użytkownika może otrzymać ograniczony dostęp do wskazanych plików lub zdarzeń;
- narzędzie może zgłosić podejrzenie i poprosić VGK o kwarantannę;
- decyzja o operacji uprzywilejowanej należy do autorytetu bezpieczeństwa VGK;
- nie wolno przyznawać nieograniczonego dostępu do jądra, pamięci, tablic stron ani rejestrów sterujących CPU;
- każdy przyszły interfejs musi być udokumentowany, wersjonowany, ograniczony uprawnieniami i testowany pod kątem nadużyć.

Samo określenie programu jako „antywirus” nie jest ani powodem do zaufania, ani powodem do automatycznej blokady.

## 8. Wydajność, prywatność i praca offline

Docelowa architektura ma uwzględniać komputery z 4–8 GB RAM i starsze procesory x86-64:
- skanowanie powinno działać porcjami i z ograniczeniem pamięci;
- zadania intensywne mogą być planowane na czas bezczynności;
- system powinien unikać niepotrzebnego wielokrotnego skanowania niezmienionych plików;
- użytkownik powinien widzieć, gdy skanowanie wpływa na wydajność;
- lokalne decyzje bezpieczeństwa muszą działać bez chmury i bez płatnej subskrypcji;
- aktualizacje sygnatur lub reputacji mogą wymagać sieci, ale brak połączenia nie może wyłączać weryfikacji podpisów i uprawnień;
- wykluczenia skanowania, jeśli zostaną dodane, muszą być jawne, ograniczone i sygnalizować ryzyko.

Nie należy obiecywać pełnej ochrony przed nowym złośliwym oprogramowaniem wyłącznie na podstawie lokalnych sygnatur.

## 9. Etapy realizacji

| Etap | Zakres | Stan według obecnej dokumentacji repozytorium |
|---|---|---|
| 1. Podstawa zaufania startu | podpis wykazu, klucz publiczny, weryfikacja plików przed startem, odmowa przy błędzie | **Częściowo istnieje:** moduły SHA-256, Ed25519, parser VGKM i wspólna kontrola plików są opisane jako testowane; połączenie z UEFI jest przygotowane, ale nie jest wywoływane w ścieżce startowej. Brak potwierdzonego klucza wydawcy i aktywnej blokady startu |
| 2. Integralność jądra i uprawnienia | ochrona pamięci jądra, kontrola operacji uprzywilejowanych, zaufanie sterowników | **Projektowane / niezaimplementowane jako pełny mechanizm ochronny** |
| 3. Izolacja aplikacji | przestrzenie adresowe, ograniczone uprawnienia, kontrola dostępu do zasobów | **Projektowane** |
| 4. Skanowanie plików i zgodność AV | inspekcja plików, nośników, API skanerów i kwarantanny | **Projektowane** |
| 5. Kontrola sieci | zapora, reguły ruchu, ograniczone API filtrowania | **Projektowane** |
| 6. Dzienniki i odzyskiwanie | wiarygodny dziennik, SafeCore/System Naprawczy, bezpieczne przywracanie | **Projektowane; brak kompletnego działającego systemu naprawczego** |

Statusy dotyczą wyłącznie zakresu znanego z aktualnych dokumentów i przeglądu plików. Przed rozpoczęciem implementacji każdego etapu należy sprawdzić aktualny kod i testy, ponieważ dokumentacja może wyprzedzać lub nie odzwierciedlać wszystkich zmian.

## 10. Wymagane testy bezpieczeństwa

Każdy etap powinien mieć testy pozytywne, negatywne i testy zachowania po błędzie. Minimalny zestaw:

1. **Poprawny podpis i wykaz** – zaufany składnik przechodzi weryfikację.
2. **Nieprawidłowy podpis** – składnik nie zostaje użyty; weryfikacja plików nie rozpoczyna się przed sprawdzeniem podpisu.
3. **Zmieniony wykaz lub plik** – wykrycie zmiany, odmowa użycia, czytelny zapis zdarzenia.
4. **Podmienione jądro** – brak przekazania sterowania do jądra przy niezgodności.
5. **Nieautoryzowany sterownik** – odmowa załadowania i brak dostępu do chronionych zasobów.
6. **Złośliwy nośnik wymienny** – brak automatycznego uruchomienia; bezpieczna obsługa błędnych danych wejściowych i uszkodzonego systemu plików.
7. **Cofnięcie wersji** – odmowa niedozwolonego powrotu do podatnej wersji, także gdy jej podpis jest poprawny.
8. **Odzyskiwanie po awarii** – przerwanie nieudanej aktualizacji lub naprawy bez uszkodzenia danych użytkownika.
9. **Fałszywe alarmy** – legalny, poprawnie podpisany plik i zgodne narzędzie ochronne nie są bezpodstawnie blokowane.
10. **Nadużycie uprawnień** – aplikacja nie może uzyskać dostępu do pamięci jądra, danych innego procesu ani kluczy zaufania.
11. **Brak Internetu** – lokalne sprawdzanie podpisów, uprawnień i integralności nadal działa.
12. **Ograniczone zasoby** – skanowanie nie może doprowadzić do niekontrolowanego zużycia RAM lub zawieszenia systemu.
13. **Testy odporności parserów** – błędne i skrajne dane wejściowe, fuzzing oraz kontrola limitów rozmiarów.
14. **Dziennik zdarzeń** – sprawdzenie, czy krytyczne zdarzenia są zapisywane i czy aplikacja użytkownika nie może ich po cichu zmienić.

Wynik testu w emulatorze nie jest równoznaczny z potwierdzeniem działania na fizycznym sprzęcie. Każdy raport musi wskazywać środowisko, wersję kodu i dokładny zakres testu.

## 11. Warunki dopuszczenia funkcji bezpieczeństwa

Funkcja nie może zostać uznana za gotową tylko dlatego, że kod się kompiluje. Przed uznaniem jej za wdrożoną należy:
- opisać chroniony zasób i model uprawnień;
- sprawdzić wszystkie ścieżki błędów i zachowanie przy braku danych;
- wykonać testy negatywne oraz testy nadużyć;
- ocenić wpływ na wydajność i możliwość odzyskania;
- sprawdzić, czy awaria mechanizmu ochronnego nie powoduje cichego przejścia do trybu niezabezpieczonego;
- udokumentować ograniczenia i poziom testów.

Stany raportowania: **zaprojektowane**, **częściowo zaimplementowane**, **zaimplementowane**, **sprawdzone statycznie**, **sprawdzone w emulatorze**, **sprawdzone na fizycznym sprzęcie**. Nie należy łączyć tych stanów w jedno ogólne określenie „działa”.

## 12. Ograniczenia modelu

Żaden system operacyjny nie może zagwarantować pełnej ochrony, jeśli atakujący kontroluje firmware, sprzęt, klucz wydawcy lub fizycznie przejął urządzenie i wszystkie mechanizmy ochronne. VGK powinien jasno wskazywać te granice.

Model nie zastępuje analizy konkretnej implementacji, przeglądu kodu, testów penetracyjnych ani audytu kryptograficznego. Jest dokumentem żywym, który należy rozwijać wraz z architekturą VGK.
