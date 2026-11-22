# Kontrakt uprawnień Inteligentnego Opiekuna Systemu

**Status:** propozycja architektoniczna. Dokument definiuje przyszłe granice uprawnień IOS. Nie oznacza, że IOS lub opisane mechanizmy są już zaimplementowane.

## 1. Cel

Kontrakt określa, jakie działania Inteligentny Opiekun Systemu (IOS) może inicjować, jakie decyzje może podejmować samodzielnie, kiedy potrzebuje zgody administratora i których operacji nie może wykonywać w zwykłym trybie.

Podstawowa zasada: **uprawnienie do ochrony systemu nie jest uprawnieniem do dowolnego zarządzania systemem**.

IOS ma chronić integralność, dostępność i dane VGK. Nie może uzyskać nieograniczonej władzy nad jądrem, sprzętem ani plikami tylko dlatego, że pełni funkcję nadrzędnego opiekuna.

## 2. Hierarchia autorytetu

Docelowa hierarchia odpowiedzialności:

1. **Jądro VGK** – egzekwuje ochronę pamięci, izolację i operacje uprzywilejowane. Żaden proces użytkownika, w tym IOS, nie może samodzielnie ominąć tych reguł.
2. **Zaufana polityka systemowa** – określa nieprzekraczalne granice, dozwolone operacje i warunki zgody. Jej zmiana wymaga kontrolowanej, uwierzytelnionej procedury.
3. **Silnik Polityki Bezpieczeństwa IOS** – ocenia żądania w granicach polityki i wydaje ograniczone decyzje.
4. **Moduły wykonawcze IOS** – wykonują wyłącznie konkretne, autoryzowane operacje.
5. **Administrator i użytkownik** – mogą inicjować dozwolone czynności i zatwierdzać działania wymagające zgody; nie mogą zwykłym poleceniem nadać aplikacji lub sterownikowi nieograniczonego dostępu do jądra.
6. **Heurystyki i AI** – analizują, wyjaśniają i rekomendują. Nie są źródłem uprawnień ani autorytetem kryptograficznym.

Hierarchia nie oznacza, że IOS może wydawać jądru dowolne polecenia. Jądro weryfikuje każde żądanie niezależnie i odrzuca operacje spoza przyznanego interfejsu.

## 3. Klasy uprawnień

| Klasa | Zakres | Zasada |
|---|---|---|
| P0 – obserwacja | odczyt dozwolonego stanu, diagnostyka, raportowanie | domyślny zakres modułów monitorujących |
| P1 – reakcja odwracalna | czasowe ograniczenie procesu, blokada połączenia, odmowa uruchomienia | możliwa automatycznie, jeśli reguła jest wcześniej zatwierdzona |
| P2 – zmiana systemowa ograniczona | naprawa pliku systemowego z wiarygodnego źródła, instalacja zatwierdzonego pakietu | wymaga kontrolowanej procedury i weryfikacji wyniku |
| P3 – operacja wrażliwa | usunięcie danych, przywrócenie kopii nadpisującej pliki, zmiana klucza zaufania, trwała zmiana polityki | wymaga jawnej zgody administratora i dodatkowych zabezpieczeń |
| P4 – operacja zabroniona | samodzielne wyłączenie ochrony jądra, dowolny zapis do pamięci jądra, instalacja niezweryfikowanego kodu uprzywilejowanego | niedozwolona przez zwykły interfejs IOS |

P4 nie jest zwykłym „brakiem zgody”. To granica architektury. Jeżeli wyjątkowa procedura serwisowa będzie kiedyś konieczna, musi być zaprojektowana oddzielnie, jawnie, z uwierzytelnieniem, ograniczeniem zakresu i audytem.

## 4. Macierz działań

| Działanie | Samodzielnie IOS | Zgoda administratora | Niedozwolone |
|---|---:|---:|---:|
| Odczyt stanu systemu w przyznanym zakresie | Tak | Nie | Nie |
| Zapis diagnostyczny do chronionego dziennika | Tak | Nie | Nie |
| Odmowa uruchomienia składnika z nieprawidłowym podpisem | Tak | Nie | Nie |
| Odmowa załadowania nieautoryzowanego sterownika | Tak | Nie | Nie |
| Czasowe odizolowanie podejrzanego procesu | Tak, według reguły | Zależnie od skutków | Nie |
| Czasowe zablokowanie podejrzanego połączenia | Tak, według reguły | Zależnie od skutków | Nie |
| Kwarantanna pliku z możliwością przywrócenia | Tylko przy spełnionych warunkach i zachowaniu oryginału | W pozostałych przypadkach | Nie |
| Trwałe usunięcie pliku użytkownika | Nie | Tak, po przedstawieniu skutków | Nie |
| Naprawa pliku systemowego z podpisanego źródła | Ograniczona, po zabezpieczeniu stanu | Gdy naprawa może wpłynąć na dane lub sprzęt | Nie |
| Przywrócenie kopii nadpisującej dane | Nie | Tak | Nie |
| Cofnięcie aktualizacji systemowej | Tylko wcześniej zatwierdzona, bezpieczna ścieżka bez utraty danych | W pozostałych przypadkach | Nie |
| Zmiana klucza zaufania lub klucza wydawcy | Nie | Tak, przez osobną procedurę | Nie przez zwykłe API |
| Nadanie aplikacji praw jądra | Nie | Nie przez zwykły interfejs | Tak |
| Bezpośrednia modyfikacja tablic stron lub rejestrów CPU przez IOS | Nie | Nie | Tak |
| Wyłączenie krytycznych kontroli integralności w celu obejścia błędu | Nie | Nie przez zwykły interfejs | Tak |

„Samodzielnie” oznacza wyłącznie działanie w ramach wcześniej zatwierdzonej polityki, ograniczonego interfejsu i przetestowanego mechanizmu. Nie jest zgodą na dowolną interpretację celu.

## 5. Warunki automatycznej reakcji

Przed automatyczną reakcją IOS musi sprawdzić:

1. Czy źródło zdarzenia i dane wejściowe zostały poprawnie zweryfikowane?
2. Czy istnieje jednoznaczna reguła polityki obejmująca ten przypadek?
3. Czy działanie mieści się w przyznanych uprawnieniach?
4. Czy skutek jest odwracalny lub ograniczony?
5. Czy działanie nie grozi utratą danych użytkownika?
6. Czy istnieje możliwość zapisania decyzji i wyniku?
7. Czy awaria wykonania pozostawi system w bezpiecznym stanie?

Jeżeli odpowiedź na krytyczne pytanie jest nieznana, IOS powinien odmówić ryzykownej operacji, zachować dostępne dane diagnostyczne i wyjaśnić przyczynę. Nie może traktować braku informacji jako zgody.

## 6. Zgoda administratora

Zgoda musi być:
- jawna i przypisana do uwierzytelnionej sesji administratora;
- udzielona dla konkretnej operacji, zasobu i zakresu;
- poprzedzona czytelnym opisem skutków i ryzyka;
- ograniczona czasowo i jednorazowa, chyba że polityka wyraźnie stanowi inaczej;
- zapisana w dzienniku wraz z wynikiem operacji.

Nie wolno używać ogólnego okna „Zezwól na wszystko”. Zgoda na jedną naprawę nie daje prawa do kolejnych napraw, trwałego usuwania plików ani zmiany polityki.

Jeśli administrator nie jest dostępny, IOS może wykonać wcześniej zatwierdzone działania ochronne o ograniczonym, odwracalnym skutku. Nie może w zastępstwie administratora zatwierdzić operacji destrukcyjnej.

## 7. Sytuacje awaryjne

W sytuacji aktywnego zagrożenia IOS może bez dodatkowego pytania:
- odmówić uruchomienia niezweryfikowanego składnika;
- zatrzymać lub ograniczyć podejrzany proces, jeżeli mechanizm jest bezpieczny;
- czasowo odizolować połączenie;
- przejść do wcześniej zdefiniowanego trybu ograniczonego;
- zabezpieczyć dziennik i przygotować raport.

Nie może pod pozorem sytuacji awaryjnej:
- usuwać danych użytkownika;
- formatować nośników;
- zmieniać kluczy zaufania;
- instalować niezweryfikowanego kodu;
- wyłączać ochrony jądra;
- przyznawać sobie nowych uprawnień.

Jeżeli bezpieczne działanie nie jest możliwe, system powinien zatrzymać zagrożoną operację i przejść do kontrolowanego trybu odzyskiwania, o ile taki tryb jest dostępny i zweryfikowany.

## 8. Rozdzielenie decyzji i wykonania

Każde żądanie powinno przechodzić przez następujący schemat:

1. Moduł wykrywający przekazuje ustrukturyzowane zdarzenie.
2. Walidator sprawdza format, źródło, rozmiary i kompletność danych.
3. Silnik Polityki Bezpieczeństwa ocenia zdarzenie i kontekst.
4. Polityka zwraca decyzję: zezwól, odmów, ogranicz odwracalnie albo wymagaj zgody.
5. Kontrolowany wykonawca otrzymuje tylko konkretną operację i niezbędne parametry.
6. Jądro ponownie weryfikuje uprawnienia dla operacji uprzywilejowanej.
7. Dziennik zapisuje obserwację, decyzję, wykonawcę i rezultat.

Żaden etap nie może przekazywać dowolnego tekstu, skryptu lub danych z niezaufanego źródła jako polecenia do wykonania.

## 9. Odporność na błędy i nadużycia

- Brak odpowiedzi Silnika Polityki nie oznacza zgody.
- Uszkodzony lub niekompletny wynik weryfikacji oznacza odmowę operacji chronionej.
- Awaria modułu diagnostycznego nie może wyłączać niezależnej ochrony jądra.
- Zwykła aplikacja nie może podszyć się pod IOS ani podsłuchiwać kanału administracyjnego.
- Moduły IOS nie mogą nadawać uprawnień innym modułom bez zatwierdzonej polityki.
- Zmiana uprawnień musi być wersjonowana, uwierzytelniona i rejestrowana.
- Operacje naprawcze muszą mieć limity czasu, rozmiaru, pamięci i zakresu.
- Błędy dziennika muszą być widoczne; nie wolno po cichu raportować sukcesu bez zapisu wymaganego zdarzenia.

## 10. Dziennik decyzji

Dla każdej istotnej operacji należy zapisać co najmniej:
- identyfikator zdarzenia i czas;
- źródło zgłoszenia;
- zasób i zakres operacji;
- wersję zastosowanej polityki;
- decyzję i uzasadnienie;
- informację, czy wymagana była zgoda i kto ją zatwierdził (bez zbędnego przechowywania danych osobowych);
- wykonawcę i wynik;
- informację o błędzie lub przerwaniu.

Dziennik nie powinien zawierać haseł, kluczy prywatnych ani pełnych treści prywatnych plików. Zwykłe aplikacje nie mogą go dowolnie modyfikować.

## 11. Testy wymagane przed wdrożeniem

1. Próba uzyskania przez zwykłą aplikację uprawnień IOS – odmowa.
2. Próba podszycia się pod moduł IOS – odmowa.
3. Brak odpowiedzi polityki – odmowa operacji chronionej.
4. Błędne lub spreparowane zdarzenie – walidacja i bezpieczna odmowa.
5. Nieprawidłowy podpis składnika – brak uruchomienia lub załadowania.
6. Próba trwałego usunięcia danych bez zgody – odmowa.
7. Zgoda na jedną operację i próba wykorzystania jej do innej – odmowa.
8. Próba zmiany klucza zaufania przez zwykły interfejs – odmowa.
9. Awaria wykonawcy w połowie naprawy – kontrolowane przerwanie i sprawdzenie integralności.
10. Awaria dziennika – jawny błąd, bez fałszywego raportu pełnego sukcesu.
11. Utrata łączności sieciowej – lokalna polityka nadal działa.
12. Testy obciążeniowe na ograniczonych zasobach – brak niekontrolowanego zużycia pamięci.

Testy muszą być uruchamiane dla konkretnego commita i środowiska. Wynik statyczny, emulator i fizyczny komputer należy raportować osobno.

## 12. Stan i kolejność wdrożenia

Kontrakt jest dokumentem projektowym. Nie przyznaje obecnemu kodowi VGK nowych uprawnień.

Zalecana kolejność:
1. zatwierdzić model zagrożeń i architekturę IOS;
2. zaprojektować format zdarzeń i dziennika;
3. zaprojektować mały, wersjonowany interfejs jądro–IOS;
4. wdrożyć tylko odczytowy raport stanu;
5. zaimplementować deterministyczny Silnik Polityki i testy macierzy uprawnień;
6. dodać pojedyncze, odwracalne reakcje;
7. dopiero później rozważać ograniczone naprawy i operacje wymagające zgody.

Nie należy implementować szerokich uprawnień IOS przed istnieniem testowalnego interfejsu, dziennika i polityki egzekwowanej przez jądro.


## 13. Kierunek produktu: autonomiczny Opiekun systemu

**Decyzja kierunkowa z 2026-11-22:** użytkownik VGK nie powinien być zmuszony do codziennego administrowania ochroną, sterownikami, aktualizacjami ani naprawami. IOS ma wykonywać te obowiązki samodzielnie w granicach zaufanej, wersjonowanej polityki systemowej. Interfejs użytkownika jest odczytowy; nie jest panelem nadawania uprawnień ani ręcznego sterowania modułami.

### 13.1. Uprawnienia systemowe IOS

IOS jest projektowany jako zaufana usługa systemowa z wąskimi, jawnymi uprawnieniami do zarządzania VGK. Oznacza to więcej możliwości niż zwykła aplikacja użytkownika, ale nie nieograniczony dostęp. Jądro nadal egzekwuje granice pamięci, izolacji i operacji uprzywilejowanych. Uprawnienia IOS wynikają z roli i polityki VGK, a nie z decyzji modelu AI ani dowolnego polecenia tekstowego.

### 13.2. Automatyczne działania bez angażowania użytkownika

Po wdrożeniu i przetestowaniu odpowiednich mechanizmów IOS powinien samodzielnie:
- blokować uruchomienie składnika, którego integralność lub zaufanie nie zostały potwierdzone;
- izolować podejrzany plik lub proces, gdy istnieje wiarygodna podstawa i bezpieczna procedura;
- unieszkodliwiać potwierdzone złośliwe składniki;
- usuwać potwierdzone złośliwe składniki z aktywnego systemu po sprawdzeniu ich tożsamości, zakresu i skutków;
- wykonywać zatwierdzone naprawy plików systemowych i konserwację;
- instalować zweryfikowane aktualizacje w kontrolowanym trybie;
- sprawdzać rezultat każdej operacji i zapisywać go w Dzienniku Decyzji.

Dla ochrony przed fałszywym alarmem plik jedynie nieznany lub podejrzany nie może być automatycznie uznany za potwierdzone malware. Wątpliwy obiekt należy najpierw zablokować lub poddać kwarantannie. Kwarantanna ma zachowywać możliwość bezpiecznego odzyskania obiektu, jeżeli późniejsza weryfikacja wykaże pomyłkę. Trwałe usunięcie ma dotyczyć potwierdzonego złośliwego obiektu i podlegać ścisłym testom zakresu oraz ochrony danych.

### 13.3. Kiedy IOS pyta użytkownika

Pytania powinny być rzadkie i dotyczyć przede wszystkim skutków dla pracy użytkownika, a nie technicznej obsługi systemu. Przykład: „Komputer wymaga ponownego uruchomienia, aby dokończyć aktualizację. Czy uruchomić go ponownie teraz?”. Użytkownik może odroczyć restart. IOS powinien przygotować operację, zapisać stan i wznowić ją w bezpiecznym momencie.

IOS nie powinien pytać o rutynowe, wcześniej zatwierdzone działania ochronne. Nie może jednak samowolnie kasować prywatnych danych, formatować nośników ani wykonywać operacji o nieprzewidywalnych skutkach. W takich przypadkach ma zastosować bezpieczne ograniczenie, zachować dane i przejść do kontrolowanej ścieżki odzyskiwania lub serwisowej.

### 13.4. Rola AI i automatyzacji

AI może pomagać IOS łączyć zdarzenia, analizować symptomy, wybierać kolejność diagnostyki i objaśniać wyniki. Nie otrzymuje bezpośredniego, nieograniczonego dostępu do jądra ani możliwości wykonywania dowolnego kodu. Konkretne działanie wykonuje ograniczony moduł IOS na podstawie sprawdzalnej polityki i uprawnień egzekwowanych przez jądro. Podstawowa ochrona musi działać lokalnie, także bez Internetu i bez dostępności modelu AI.

### 13.5. Historia zamiast panelu administracyjnego

Użytkownik widzi zwięzły, odczytowy zapis rezultatów, np. „Szkodliwy plik został unieszkodliwiony i usunięty z aktywnego systemu”, „Naprawiono składnik VGK” albo „Aktualizacja gotowa – wymagany restart”. Historia musi rozróżniać wykrycie, decyzję, wykonanie i potwierdzony wynik. Nie wolno raportować sukcesu, jeśli wykonanie lub weryfikacja nie powiodły się.

Ta decyzja zmienia domyślny model interakcji z użytkownikiem, ale nie znosi testów, audytu, ograniczeń uprawnień ani ochrony danych. Autonomia oznacza samodzielne wykonywanie zatwierdzonych obowiązków, a nie niekontrolowaną władzę.
