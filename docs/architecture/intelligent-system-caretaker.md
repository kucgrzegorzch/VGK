# Inteligentny Opiekun Systemu (IOS)

**Status:** propozycja architektury. IOS nie jest obecnie gotowym ani działającym podsystemem VGK. Dokument określa jego przyszłą rolę, granice uprawnień i sposób współpracy z jądrem.

## 1. Nazwa i rola

**Inteligentny Opiekun Systemu (IOS)** jest docelową, nadrzędną usługą systemową VGK, która nadzoruje stan systemu, koordynuje diagnostykę, egzekwowanie zatwierdzonych zasad bezpieczeństwa, aktualizacje i odzyskiwanie sprawności.

„Bezpieczeństwo systemu” jest jednym z obszarów odpowiedzialności IOS, a nie nazwą całego Opiekuna. IOS nie jest zwykłym antywirusem, aplikacją użytkownika ani zewnętrzną usługą chmurową.

Nazwa „inteligentny” oznacza zdolność łączenia informacji diagnostycznych, wykrywania anomalii i przedstawiania zaleceń. Nie oznacza, że decyzje o krytycznych uprawnieniach mogą być podejmowane przez niekontrolowany model AI.

## 2. Nadrzędne zasady

1. **Ochrona przed wygodą:** IOS nie może obniżyć krytycznej ochrony tylko po to, aby operacja zakończyła się powodzeniem.
2. **Minimalne uprawnienia:** każdy składnik IOS otrzymuje wyłącznie uprawnienia potrzebne do swojej funkcji.
3. **Rozdzielenie wykrywania od wykonania:** skaner lub moduł diagnostyczny może zgłosić problem; nie otrzymuje przez to prawa do dowolnej naprawy.
4. **Bezpieczna odmowa:** gdy nie ma wystarczających danych do bezpiecznej decyzji, IOS zatrzymuje ryzykowną operację i wyjaśnia przyczynę.
5. **Ochrona danych:** automatyczne reakcje preferują działania odwracalne. Trwałe kasowanie i nadpisywanie danych wymagają osobnej procedury.
6. **Jawność:** ważne decyzje, ich podstawa i wynik są zapisywane w chronionym dzienniku.
7. **Działanie lokalne:** podstawowe kontrole muszą działać bez Internetu i bez płatnej usługi.
8. **Kontrolowana zmiana:** reguły, klucze zaufania i uprawnienia krytyczne zmienia się wyłącznie przez uwierzytelnioną procedurę.

## 3. Podział architektury

IOS powinien być logicznie podzielony na następujące części. Nie przesądza to jeszcze, czy każda część będzie osobnym procesem, biblioteką czy usługą jądra; taki wybór wymaga osobnego projektu technicznego.

| Część | Odpowiedzialność | Granica uprawnień |
|---|---|---|
| Koordynator Opiekuna | zbiera wyniki modułów, ustala kolejność działań i prezentuje stan | nie może sam omijać polityki ani uzyskiwać bezpośredniego dostępu do pamięci jądra |
| Silnik Polityki Bezpieczeństwa | ocenia żądania według jawnych, wersjonowanych reguł | jedyne miejsce podejmowania decyzji polityki; nie wykonuje dowolnych poleceń z treści zgłoszeń |
| Ochrona Integralności | sprawdza podpisy, manifesty i zgodność składników | raportuje wynik; nie modyfikuje samowolnie zaufanych kluczy |
| Diagnostyka i Monitor Stanu | wykrywa błędy sprzętu, zasobów, usług i systemu plików | odczyt w zakresie przyznanych uprawnień; działania naprawcze osobno autoryzowane |
| Ochrona Plików i Aplikacji | kontroluje uruchamianie, pliki, nośniki i zgłoszenia skanerów | brak prawa do bezwarunkowego kasowania danych |
| Kontrola Sterowników i Urządzeń | sprawdza pochodzenie, zgodność i uprawnienia sterowników | decyzja o dopuszczeniu oparta na polityce i weryfikacji, nie na samej nazwie producenta |
| Kontrola Sieci | stosuje zatwierdzone reguły ruchu i reaguje na naruszenia | reguły ograniczone; blokady powinny być możliwie odwracalne |
| Menedżer Aktualizacji | weryfikuje pakiety, wersje, zależności i możliwość wycofania | nie instaluje pakietu bez sprawdzenia autentyczności i integralności |
| System Odzyskiwania | uruchamia bezpieczne procedury naprawy i przywracania | nie może wyłączać zabezpieczeń ani nadpisywać danych bez wymaganej zgody |
| Dziennik Decyzji IOS | jeden wspólny, dopisywany dziennik audytowy: obserwacje, walidacje, decyzje, zgody, wykonania i wyniki | zapis chroniony przed zwykłymi aplikacjami; ograniczanie danych wrażliwych |

**Ujednolicenie dziennika:** obserwacje zbierane przez monitor nie tworzą osobnego magazynu. Są zapisywane jako odpowiednie typy wpisów w jednym Dzienniku Decyzji IOS, zgodnie z `ios-decision-audit-log.md`. Strumień zdarzeń i kolektor są warstwą wejściową, a nie odrębnym dziennikiem.

## 4. Kto podejmuje decyzje

Należy rozdzielić cztery role:

- **Moduł wykrywający** – dostarcza obserwację, np. „plik ma nieprawidłowy podpis”.
- **Silnik Polityki Bezpieczeństwa** – sprawdza obserwację względem reguł i określa dozwoloną reakcję.
- **Kontrolowany wykonawca** – wykonuje wyłącznie konkretną, zatwierdzoną operację.
- **Użytkownik lub administrator** – zatwierdza czynności wymagające zgody i może zapoznać się z uzasadnieniem.

Żaden moduł wykrywający nie może sam nadać sobie uprawnień wykonawczych. Komunikaty, nazwy plików i dane pochodzące z niezaufanych źródeł są danymi, a nie poleceniami dla Opiekuna.

## 5. Rola algorytmów inteligentnych i AI

W przyszłości IOS może używać metod heurystycznych lub modeli AI do:
- grupowania powiązanych zdarzeń;
- wskazywania nietypowych zachowań;
- porównywania symptomów awarii;
- proponowania kolejności diagnostyki;
- wyjaśniania użytkownikowi możliwych przyczyn i skutków.

Wynik heurystyki lub AI jest **rekomendacją**, a nie uprawnieniem. Nie może samodzielnie:
- zmienić kluczy zaufania;
- zapisać do pamięci jądra lub rejestrów sterujących CPU;
- wyłączyć ochrony integralności;
- trwale usuwać plików użytkownika;
- instalować niezweryfikowanego sterownika lub aktualizacji;
- zmieniać polityki bezpieczeństwa poza zatwierdzonym procesem.

Krytyczne decyzje muszą być sprawdzalne i oparte na deterministycznych regułach. Awaria lub niedostępność modelu AI nie może wyłączać podstawowej ochrony VGK. System nie może wymagać przesyłania prywatnych plików do zewnętrznej usługi, aby wykonać podstawową kontrolę.

## 6. Poziomy reakcji

| Poziom | Przykład | Docelowe zachowanie |
|---|---|---|
| Informacyjny | ostrzeżenie o wysokim użyciu pamięci | poinformować i zapisać zdarzenie |
| Ograniczenie odwracalne | podejrzany proces lub połączenie | czasowo ograniczyć, zachować możliwość przywrócenia |
| Blokada ochronna | nieprawidłowy podpis pliku startowego | odmówić użycia składnika; nie przechodzić po cichu w tryb niezabezpieczony |
| Naprawa kontrolowana | uszkodzony plik systemowy | zweryfikować źródło, zabezpieczyć stan, wykonać minimalną naprawę i sprawdzić wynik |
| Operacja wymagająca zgody | usunięcie danych, cofnięcie aktualizacji z ryzykiem utraty danych | przedstawić skutki i wymagać jawnej zgody |

W sytuacji bezpośredniego zagrożenia IOS może natychmiast zastosować wcześniej zatwierdzoną, odwracalną blokadę. Nie może rozszerzać jej samodzielnie na trwałe działania destrukcyjne.

## 7. Interfejs z jądrem

Jądro pozostaje ostatecznym egzekutorem ochrony pamięci i operacji uprzywilejowanych. IOS nie może być „drugim jądrem” ani uzyskać nieograniczonego dostępu tylko dlatego, że jest komponentem zaufanym.

Przyszły interfejs jądro–IOS powinien:
- być mały, jawnie określony i wersjonowany;
- weryfikować uprawnienia każdego żądania;
- walidować wszystkie wskaźniki, długości, identyfikatory i zakresy;
- nie przyjmować wskaźników do arbitralnego wykonywania kodu;
- ograniczać częstotliwość i rozmiar żądań;
- zapewniać bezpieczne zachowanie przy błędzie lub awarii IOS;
- nie udostępniać aplikacjom zwykłym bezpośredniego kanału do funkcji uprzywilejowanych.

Awaria IOS nie może powodować, że jądro automatycznie zezwoli na operacje wcześniej zabronione. Jądro musi zachować własne minimalne mechanizmy ochronne.

## 8. Tryby działania

Docelowo Opiekun powinien pracować w trybach:
- **Normalny** – ciągły nadzór o małym koszcie zasobów.
- **Diagnostyczny** – dokładniejsze zbieranie danych po wykryciu problemu lub na żądanie użytkownika.
- **Ograniczony** – blokowanie ryzykownych operacji, gdy stan systemu nie pozwala na bezpieczną pracę.
- **Odzyskiwania** – minimalne środowisko do weryfikacji i naprawy systemu.
- **Konserwacji** – kontrolowane aktualizacje i czynności administracyjne.

Przejście między trybami musi być jawne w dzienniku. Tryb ograniczony nie może być sposobem na trwałe ukrycie awarii.

## 9. Wydajność i niezawodność

- Nadzór podstawowy powinien być lekki i działać także na komputerach z 4–8 GB RAM.
- Intensywne skanowanie powinno mieć limity pamięci, czasu i priorytetu.
- Długie zadania powinny być dzielone na porcje i możliwe do wstrzymania.
- IOS nie może blokować uruchomienia komputera bez uzasadnionej potrzeby; krytyczna kontrola startu jest jednak ważniejsza niż wygoda.
- Moduły powinny być możliwie niezależne, aby awaria diagnostyki nie wyłączała kontroli podpisów.
- Każda automatyczna naprawa musi mieć punkt kontroli wyniku i zdefiniowaną ścieżkę przerwania.

## 10. Etapy wdrożenia

1. **Kontrakt i granice zaufania** – role, interfejsy, uprawnienia, format zdarzeń i polityka decyzji.
2. **Podstawowy dziennik i raport stanu** – tylko odczyt, bez automatycznych napraw.
3. **Silnik Polityki Bezpieczeństwa** – deterministyczne reguły i testy decyzji na danych testowych.
4. **Integracja ochrony integralności** – wykorzystanie istniejących modułów kryptograficznych po potwierdzeniu ich rzeczywistego podłączenia i testów.
5. **Diagnostyka sprzętu i systemu** – bezpieczne, ograniczone odczyty i raportowanie.
6. **Kontrolowane reakcje** – najpierw odwracalne blokady, potem wąsko określone naprawy.
7. **Odzyskiwanie, aktualizacje i skanowanie** – dopiero po zapewnieniu wiarygodnego dziennika, uprawnień i testów awarii.
8. **Metody heurystyczne/AI** – wyłącznie jako warstwa wspomagająca, po zabezpieczeniu podstawowych reguł.

Każdy etap wymaga osobnego przeglądu kodu, testów negatywnych i oceny wpływu na dane użytkownika. Sam dokument nie oznacza wdrożenia żadnego z tych etapów.

## 11. Minimalne kryteria akceptacji

- Moduł wykrywający nie może wykonać operacji uprzywilejowanej bez decyzji polityki.
- Zwykła aplikacja nie może podszyć się pod Opiekuna ani wywołać jego wewnętrznych funkcji.
- Nieprawidłowy lub brakujący wynik weryfikacji nie może być interpretowany jako sukces.
- AI/heurystyka nie może samodzielnie zatwierdzić operacji destrukcyjnej.
- Dziennik musi rozróżniać obserwację, decyzję, wykonanie i rezultat.
- Awaria jednego modułu nie może wyłączać niezależnych zabezpieczeń.
- Naprawa musi być przerywalna i nie może po cichu nadpisywać danych użytkownika.
- Podstawowe reguły muszą działać offline.
- Testy muszą obejmować fałszywe alarmy, nadużycie uprawnień, błędne dane wejściowe, awarię modułu i przerwanie naprawy.

## 12. Stan obecny

Na dzień utworzenia tej propozycji IOS jest **koncepcją architektoniczną**, nie zaimplementowaną usługą systemową. Nie ma podstaw, aby twierdzić, że VGK posiada już działającego autonomicznego Opiekuna, aktywny antywirus, pełny mechanizm naprawczy lub AI zarządzające systemem.

Dokument należy rozwijać wspólnie z modelem zagrożeń VGK, polityką zgodności oprogramowania ochronnego i głównymi założeniami systemowymi.


## 13. Reakcje na awarie krytyczne i komunikacja z użytkownikiem

**Decyzja produktu z 2026-11-23:** IOS samodzielnie diagnozuje i obsługuje awarie w granicach zatwierdzonej polityki. Użytkownik otrzymuje zrozumiały komunikat o wykrytym problemie, podjętej próbie i rzeczywistym wyniku. Potwierdzenie przeczytania komunikatu nie jest zgodą na działanie i nie może sterować mechanizmem ochronnym.

### 13.1. Awaria elementu krytycznego dla chłodzenia

W przypadku wiarygodnie wykrytej awarii wentylatora procesora IOS powinien:
1. zweryfikować sygnały awarii i odczyty temperatury, uwzględniając dostępność oraz wiarygodność czujników;
2. wykonać dozwolone, bezpieczne próby diagnostyczne lub naprawcze;
3. jeżeli awarii nie można usunąć, poinformować użytkownika o konieczności fizycznej wymiany wentylatora;
4. doprowadzić do kontrolowanego wyłączenia komputera, zanim temperatura osiągnie niebezpieczny próg, zgodnie z zatwierdzonymi limitami sprzętu i procesora.

Czas wyświetlany użytkownikowi, np. „około 2 minuty i 50 sekund”, jest jedynie prognozą opartą na bieżącej temperaturze i jej tempie zmian. Nie może być traktowany jako stały czas oczekiwania ani minimalne opóźnienie. Jeśli temperatura rośnie szybciej, wyłączenie następuje wcześniej. Jeśli czujnik lub dane są niewiarygodne, system wybiera bezpieczną reakcję przewidzianą dla stanu niepewnego; nie może zakładać, że chłodzenie działa prawidłowo.

Naciśnięcie przycisku potwierdzenia komunikatu ani brak reakcji użytkownika nie zmieniają decyzji o wyłączeniu. Mechanizm ochrony termicznej musi działać niezależnie od interfejsu graficznego, a istniejące zabezpieczenia sprzętowe i firmware (np. mechanizmy termicznego odcięcia procesora) pozostają aktywne i nie mogą być zastępowane przez IOS.

### 13.2. Granice automatycznej naprawy

IOS może samodzielnie wykonywać wyłącznie działania objęte zatwierdzoną polityką i ograniczonymi uprawnieniami. Niepowodzenie naprawy sprzętowej nie upoważnia go do deklarowania naprawy ani do podejmowania niebezpiecznych prób obejścia zabezpieczeń. Jeżeli wymagane jest fizyczne serwisowanie, IOS informuje o tym wprost.

### 13.3. Zależność od interfejsu

Interfejs prezentuje stan i wynik, ale nie jest elementem krytycznej pętli ochronnej. Awaria, zawieszenie lub brak odpowiedzi interfejsu nie może blokować wyłączenia termicznego ani innych działań wymaganych przez politykę. Szczegóły potwierdzania komunikatów opisuje kontrakt interfejsu IOS.


## 14. Diagnostyka kondycji podzespołów i komunikowanie awarii

**Decyzja produktu z 2026-11-25:** IOS musi wyraźnie odróżniać awarię urządzenia od pogorszenia kondycji podzespołu, który nadal działa.

### 14.1. Awaria lub niesprawność urządzenia

Jeżeli urządzenie przestaje działać albo występuje rzeczywista usterka wymagająca uwagi użytkownika (np. mikrofon przestaje działać), IOS wyświetla krótki komunikat. Komunikat opisuje wykryty problem, podjęte próby oraz ich rzeczywisty, zweryfikowany wynik. Zdarzenie pozostaje zapisane w Historii, również po zakończeniu naprawy.

Sama niekrytyczna klasyfikacja awarii nie oznacza, że IOS ma przemilczeć fakt, iż urządzenie przestało działać. Kolor wpisu w Historii i decyzja o wyświetleniu komunikatu są odrębnymi kwestiami.

### 14.2. Pogorszenie kondycji działającego podzespołu

Jeżeli podzespół nadal działa, ale diagnostyka wykrywa pogarszającą się kondycję (np. dysk wykazuje oznaki zużycia), IOS:
- nie przerywa pracy użytkownika wyskakującym komunikatem wyłącznie z powodu takiego wyniku;
- umieszcza żółty status w Historii diagnostyki;
- udostępnia w szczegółach przyczynę, wynik i wiarygodność diagnostyki, możliwe skutki oraz zalecane postępowanie;
- aktualizuje wpis na podstawie kolejnych okresowych kontroli diagnostycznych.

Żółty status jest wystarczającą sygnalizacją takiego niekrytycznego pogorszenia kondycji. Użytkownik sam otwiera Historię, kiedy chce zapoznać się ze szczegółami. IOS nie powinien wielokrotnie wyświetlać komunikatów o niezmienionym, niekrytycznym stanie.

### 14.3. Propozycja zabezpieczenia danych

Jeżeli pogorszenie kondycji dotyczy podzespołu przechowującego dane, IOS może w szczegółach diagnostyki zaproponować ich zabezpieczenie. Dopóki nie występuje bezpośrednie zagrożenie wymagające natychmiastowej reakcji ochronnej, IOS nie rozpoczyna samodzielnie kopiowania ani przenoszenia danych. Użytkownik może zabezpieczyć je samodzielnie albo wyraźnie zlecić tę czynność IOS. Po otrzymaniu polecenia IOS wykonuje wyłącznie zatwierdzony zakres działania i sprawdza wynik; nie może deklarować powodzenia bez weryfikacji.

### 14.4. Znaczenie statusów

- **Zielony** – diagnostyka potwierdza prawidłowy stan w zakresie wykonanych i wiarygodnych kontroli.
- **Żółty** – wykryto niekrytyczną nieprawidłowość lub pogorszenie kondycji; szczegóły są dostępne w Historii diagnostyki. Sam żółty status nie uruchamia wyskakującego komunikatu.
- **Czerwony** – stan krytyczny lub bezpośrednie zagrożenie; IOS podejmuje niezbędne działania ochronne zgodnie z zatwierdzoną polityką.

Kolor nie zastępuje opisu tekstowego ani symbolu. Brak danych lub brak możliwości wykonania kontroli nie może być przedstawiany jako zielony stan prawidłowy.
