# Dziennik Decyzji Inteligentnego Opiekuna Systemu (IOS)

**Status:** propozycja architektoniczna. Dokument opisuje przyszły format i zasady dziennika. Nie oznacza, że dziennik lub jego ochrona są już zaimplementowane w VGK.

## 1. Cel i zakres

Dziennik Decyzji IOS ma zapewniać możliwy do sprawdzenia zapis istotnych zdarzeń związanych z obserwacją, oceną polityki, zgodą administratora, wykonaniem operacji i jej wynikiem.

Użytkownik powinien móc odpowiedzieć na pytania:
- Co IOS wykrył i z jakiego źródła pochodziła obserwacja?
- Która wersja polityki została zastosowana?
- Jaką decyzję podjęto i z jakiego powodu?
- Czy operacja wymagała zgody, a jeśli tak, czy i w jakim zakresie ją udzielono?
- Który kontrolowany wykonawca otrzymał zadanie?
- Czy operacja została wykonana, odrzucona, przerwana lub zakończyła się błędem?
- Czy wynik został później zweryfikowany?

Dziennik jest mechanizmem audytu i diagnostyki. Nie jest samodzielnym silnikiem bezpieczeństwa i nie przyznaje IOS żadnych uprawnień.

## 2. Zasady nadrzędne

1. **Rozdzielenie etapów:** obserwacja, walidacja, decyzja, zgoda, wykonanie i wynik są odrębnymi zdarzeniami.
2. **Zapis przed działaniem:** decyzja autoryzująca operację musi zostać utrwalona przed przekazaniem zadania wykonawcy.
3. **Brak fałszywego sukcesu:** brak wpisu o wyniku nie może być przedstawiany jako potwierdzenie powodzenia.
4. **Ochrona przed manipulacją:** zwykła aplikacja ani moduł wykonawczy nie mogą dowolnie zmieniać lub usuwać wpisów.
5. **Minimalizacja danych:** zapisujemy informacje niezbędne do audytu, nie kopie prywatnych plików.
6. **Ograniczone zasoby:** dziennik ma mieć jawne limity pamięci, rozmiaru wpisu i kosztu zapisu.
7. **Czytelność:** komunikat dla użytkownika ma wyjaśniać zdarzenie zwykłym językiem, z możliwością przejścia do szczegółów technicznych.
8. **Odporność na awarie:** uszkodzenie dziennika musi być wykrywalne i nie może po cichu zmieniać decyzji polityki na „zezwól”.

## 3. Model zdarzeń

Dziennik powinien przechowywać zdarzenia, a nie jeden nadpisywany rekord stanu. Zalecane typy:

| Typ zdarzenia | Znaczenie |
|---|---|
| `observation` | moduł wykrywający zgłosił obserwację |
| `validation` | sprawdzono źródło, format i kompletność danych |
| `decision` | Silnik Polityki wydał decyzję |
| `consent_requested` | poproszono administratora o zgodę |
| `consent_granted` / `consent_denied` | zapisano odpowiedź administratora |
| `execution_started` | wykonawca rozpoczął autoryzowaną operację |
| `execution_result` | wykonanie zakończyło się określonym wynikiem |
| `verification_result` | niezależnie sprawdzono rezultat |
| `interrupted` | operację przerwano lub utracono możliwość jej kontynuacji |
| `integrity_warning` | wykryto problem z ciągłością lub integralnością dziennika |
| `policy_changed` | zmieniono wersję lub konfigurację polityki |
| `system_mode_changed` | system przeszedł między trybami pracy |

Nie każde zdarzenie wymaga osobnego wpisu na dysku. Łączenie techniczne jest dopuszczalne wyłącznie wtedy, gdy zachowuje kolejność, powiązania i możliwość odtworzenia przebiegu.

## 4. Minimalny schemat wpisu

Każdy wpis powinien zawierać następujące pola lub ich odpowiedniki:

| Pole | Opis |
|---|---|
| `schema_version` | wersja formatu wpisu |
| `event_id` | unikalny identyfikator zdarzenia |
| `sequence` | rosnący numer w obrębie dziennika |
| `boot_session_id` | identyfikator uruchomienia systemu |
| `timestamp` | czas ścienny, jeśli jest wiarygodny |
| `monotonic_time` | czas monotoniczny do porządkowania zdarzeń w sesji |
| `event_type` | typ zdarzenia z zamkniętego katalogu |
| `correlation_id` | identyfikator łączący wpisy jednego przypadku |
| `source` | moduł i wersja zgłaszająca zdarzenie |
| `subject` | typ i identyfikator zasobu, którego dotyczy sprawa |
| `policy_id` / `policy_version` | dokładna reguła i wersja polityki |
| `decision` | allow, deny, restrict, require_consent lub not_applicable |
| `reason_code` | stabilny kod przyczyny |
| `reason_summary` | krótki, bezpieczny opis uzasadnienia |
| `authority_class` | zastosowana klasa uprawnień, np. P0–P4 |
| `consent_reference` | odwołanie do wpisu zgody, jeśli dotyczy |
| `executor` | identyfikator kontrolowanego wykonawcy, jeśli uruchomiono |
| `operation_scope` | ograniczony opis dozwolonego zakresu |
| `result` | success, denied, failed, interrupted, partial lub unknown |
| `error_code` | stabilny kod błędu, jeśli wystąpił |
| `previous_entry_hash` | skrót poprzedniego wpisu w łańcuchu |
| `entry_hash` | skrót kanonicznej reprezentacji bieżącego wpisu |

Pola nieistotne dla danego typu zdarzenia mogą być pominięte. Nie wolno wypełniać brakujących danych fikcyjnymi wartościami. Nieznany czas lub niepewny zegar należy oznaczyć jawnie.

### Przykładowy przebieg logiczny

1. `observation`: moduł integralności zgłasza niezgodność skrótu pliku systemowego.
2. `validation`: źródło i dane zgłoszenia przechodzą walidację.
3. `decision`: polityka odmawia uruchomienia składnika; zapisuje kod przyczyny i wersję reguły.
4. `execution_started`: kontrolowany wykonawca otrzymuje ograniczone zadanie blokady.
5. `execution_result`: wykonawca potwierdza blokadę albo zgłasza błąd.
6. `verification_result`: osobna kontrola potwierdza, czy składnik rzeczywiście nie został uruchomiony.

Przykład jest opisem przyszłego formatu, a nie raportem z działania obecnego VGK.

## 5. Kolejność, trwałość i spójność

- Każdy wpis otrzymuje numer sekwencyjny. Luka w numeracji jest wykrywalna i wymaga wyjaśnienia.
- Zdarzenia z jednego przypadku łączy `correlation_id`; pojedyncza operacja może mieć wiele wpisów.
- Czas monotoniczny służy do ustalania kolejności w obrębie uruchomienia. Czas ścienny służy do prezentacji i może być oznaczony jako niepewny.
- Przed rozpoczęciem operacji chronionej musi zostać utrwalony wpis decyzji. Jeśli wymagany zapis nie powiedzie się, wykonawca nie może otrzymać autoryzacji.
- Gdy decyzja wymaga zgody, wpis `decision` ma status `require_consent`; samo jego zapisanie nie autoryzuje wykonania. Dopiero po uwierzytelnionej zgodzie, sprawdzeniu jej zakresu i utrwaleniu wpisu `consent_granted` polityka może wydać osobny, ograniczony wpis autoryzujący wykonanie. Odmowa lub wygaśnięcie kończy tę ścieżkę bez uruchomienia wykonawcy.
- Autoryzacja musi być związana z identyfikatorem operacji, zasobem, zakresem, wersją polityki i terminem ważności. Wykonawca sprawdza te powiązania; nie może użyć samego identyfikatora zgody jako ogólnego zezwolenia.
- Po zakończeniu wykonania należy dopisać wynik. Jeżeli system ulegnie awarii pomiędzy rozpoczęciem a wynikiem, po kolejnym uruchomieniu przypadek otrzymuje status nierozstrzygnięty/przerwany, a nie sukces.
- Zapis powinien używać bezpiecznego, odpornego na częściowy zapis formatu rekordów, z kontrolą długości, wersji i sum kontrolnych.
- Grupowanie zapisów może poprawiać wydajność, ale nie może opóźniać wymaganego wpisu autoryzacyjnego poza granicę rozpoczęcia operacji.

## 6. Integralność i granice zaufania

Docelowo dziennik powinien łączyć wpisy łańcuchem skrótów: każdy wpis obejmuje skrót poprzedniego wpisu oraz własną kanoniczną zawartość. Pozwala to wykrywać usunięcie, zmianę lub przestawienie wpisów w zakresie objętym zweryfikowanym łańcuchem.

Kanoniczna reprezentacja musi mieć jednoznacznie określoną serializację (kolejność pól, kodowanie tekstu, reprezentację liczb i wartości pustych). Obliczenie `entry_hash` obejmuje wersję schematu, identyfikator algorytmu, `previous_entry_hash` i kanoniczne pola wpisu, ale nie obejmuje samego pola `entry_hash`. Warto zastosować rozdzielenie domen dla skrótu wpisu i punktu kontrolnego, aby nie można było pomylić ich zastosowań. Algorytm i jego wersja muszą być jawne; zmiana algorytmu wymaga kontrolowanego przejścia między segmentami.

Sam łańcuch skrótów **nie jest pełną ochroną przed atakującym, który może przepisać cały nośnik i ponownie obliczyć skróty**. Dlatego przyszła implementacja powinna rozważyć:
- okresowe punkty kontrolne podpisywane kluczem niedostępnym zwykłym aplikacjom;
- ochronę klucza i licznika ciągłości przez odpowiedni mechanizm zaufania platformy, jeśli sprzęt go udostępnia;
- przechowywanie kopii punktów kontrolnych w oddzielnym, chronionym obszarze;
- wykrywanie cofnięcia starszego obrazu dziennika, jeśli dostępny jest wiarygodny licznik monotoniczny.

Nie wolno deklarować odporności na manipulację, dopóki konkretna implementacja i jej model kluczy nie zostaną zaprojektowane oraz przetestowane. Brak sprzętowego wsparcia należy raportować jako ograniczenie, a nie ukrywać.

## 7. Prywatność i minimalizacja danych

Dziennik nie powinien przechowywać:
- haseł, tokenów sesji ani kluczy prywatnych;
- pełnej treści prywatnych dokumentów, wiadomości i plików;
- niepotrzebnych danych osobowych;
- surowych danych sieciowych, jeśli wystarczy ograniczony identyfikator lub skrót.

Preferowane są identyfikatory zasobów, kody przyczyn, skróty i krótkie opisy. Skrót pliku nie zawsze jest anonimowy ani pozbawiony ryzyka korelacji; dostęp do takich danych również powinien być ograniczony.

Treść komunikatu użytkownika może być bardziej zrozumiała, ale musi pochodzić z bezpiecznych szablonów i nie może bezpośrednio interpretować niezaufanych danych jako instrukcji.

## 8. Limity, rotacja i brak miejsca

Przed implementacją należy ustalić jawne limity:
- maksymalny rozmiar pojedynczego wpisu;
- maksymalny rozmiar bieżącego dziennika;
- limit przestrzeni zarezerwowanej na zdarzenia krytyczne;
- maksymalny czas i koszt zapisu;
- zasady retencji i eksportu.

Rotacja powinna tworzyć zamknięte segmenty z numerem sekwencji, zakresem czasu, skrótem ostatniego wpisu i punktem kontrolnym integralności. Pierwszy wpis nowego segmentu musi wskazywać punkt kontrolny poprzedniego segmentu. Nie może po cichu usuwać wpisów, które są potrzebne do wyjaśnienia nierozstrzygniętej operacji.

Przy braku miejsca:
1. można usuwać lub archiwizować najstarsze, zamknięte segmenty zgodnie z jawną polityką retencji;
2. nie wolno usuwać jedynego zapisu zgody, decyzji lub wyniku operacji nadal nierozstrzygniętej;
3. należy zachować rezerwę dla zdarzeń krytycznych i samego ostrzeżenia o problemie z dziennikiem;
4. jeśli nie można utrwalić wymaganego wpisu, nowa operacja wymagająca audytu nie może się rozpocząć;
5. podstawowe mechanizmy bezpieczeństwa jądra muszą działać niezależnie od dostępności dziennika.

Awaria dziennika nie może prowadzić ani do automatycznego zezwolenia, ani do niekontrolowanego wyłączenia całego systemu. Reakcja ma być ograniczona do operacji, dla których audyt jest wymagany, i musi być jawna.

## 9. Dostęp, przegląd i eksport

Docelowy interfejs powinien oferować:
- czytelną oś czasu zdarzeń;
- filtrowanie według czasu, kategorii, zasobu i wyniku;
- widok szczegółów pokazujący regułę, uzasadnienie i zakres operacji;
- wyraźne rozróżnienie decyzji, wykonania i niezależnej weryfikacji;
- informację o lukach, uszkodzeniu lub niepewności czasu;
- eksport do udokumentowanego formatu tekstowego/ustrukturyzowanego wraz z punktami kontrolnymi integralności.

Użytkownik powinien mieć dostęp do własnych zdarzeń diagnostycznych. Zwykła aplikacja nie może modyfikować dziennika. Eksport nie może ujawniać sekretów ani danych innych użytkowników. Usuwanie lub skracanie retencji musi być kontrolowaną operacją administracyjną i samo pozostawiać odpowiedni ślad, o ile nośnik i stan systemu na to pozwalają.

## 10. Powiązanie ze zgodą administratora

Zgoda musi być osobnym zdarzeniem powiązanym z konkretną decyzją i zakresem:
- identyfikator zgody;
- uwierzytelniona rola zatwierdzającego, bez zbędnych danych osobowych;
- identyfikator operacji i zasobu;
- dokładny zakres zatwierdzenia;
- czas udzielenia i termin wygaśnięcia;
- informacja, czy zgoda była jednorazowa;
- wynik późniejszego wykonania.

Zgoda nie może być ponownie użyta dla innego zasobu, innej operacji lub szerszego zakresu. Odmowa, wygaśnięcie i anulowanie również są rejestrowane. Sam wpis w dzienniku nie zastępuje bezpiecznego uwierzytelnienia administratora.

## 11. Testy akceptacyjne

Przed uznaniem implementacji za gotową wymagane są co najmniej:

1. Poprawny przebieg obserwacja → walidacja → decyzja → wykonanie → wynik.
2. Brak wpisu decyzji z powodu błędu zapisu – wykonawca nie rozpoczyna operacji chronionej.
3. Awaria po wpisie `execution_started`, ale przed wynikiem – po restarcie zdarzenie pozostaje nierozstrzygnięte, nie oznaczone jako sukces.
4. Zmiana, usunięcie i przestawienie wpisu – wykrycie naruszenia łańcucha lub punktu kontrolnego.
5. Ucięty albo częściowo zapisany rekord – bezpieczne odzyskanie poprzednich kompletnych rekordów i jawne zgłoszenie uszkodzenia.
6. Powtórzony identyfikator lub numer sekwencji – wykrycie konfliktu.
7. Zmiana zegara systemowego – zachowanie poprawnej kolejności na podstawie czasu monotonicznego.
8. Zapełnienie limitu dziennika – rotacja zgodna z polityką albo jawna odmowa rozpoczęcia audytowanej operacji.
9. Próba zapisu lub usunięcia przez zwykłą aplikację – odmowa.
10. Zgoda na jedną operację użyta dla innego zasobu lub zakresu – odmowa.
11. Eksport – zachowanie kolejności i punktów kontrolnych bez ujawnienia sekretów.
12. Wysoka liczba zdarzeń – ograniczone zużycie RAM, CPU i operacji wejścia/wyjścia.
13. Brak Internetu – lokalny zapis, odczyt i weryfikacja nadal działają.
14. Nieprawidłowa wersja schematu – jawna obsługa niezgodności, bez interpretowania nieznanych pól jako uprawnień.

Każdy raport testowy musi wskazywać commit, konfigurację i środowisko. Test w emulatorze nie potwierdza zachowania na fizycznym komputerze.

## 12. Etapy wdrożenia

1. Zatwierdzenie schematu zdarzeń, katalogu kodów przyczyn i reguł prywatności.
2. Implementacja lokalnego, tylko dopisującego dziennika bez wpływu na decyzje systemu.
3. Testy formatu, granic rozmiaru, odzyskiwania po częściowym zapisie i rotacji.
4. Powiązanie z odczytowymi zdarzeniami diagnostycznymi.
5. Integracja z deterministycznym Silnikiem Polityki i rejestrowanie decyzji przed wykonaniem.
6. Dodanie odrębnych wpisów zgody, wykonania i niezależnej weryfikacji.
7. Dopiero po analizie modelu kluczy – punkty kontrolne i silniejsza ochrona integralności.
8. Interfejs użytkownika, filtrowanie i bezpieczny eksport.

Każdy etap wymaga osobnego przeglądu kodu i testów. Dokument nie stanowi zgody na aktywowanie operacji uprzywilejowanych.

## 13. Powiązane dokumenty

- Model zagrożeń VGK: `docs/architecture/threat-model.md`.
- Architektura Inteligentnego Opiekuna Systemu: `docs/architecture/intelligent-system-caretaker.md`.
- Kontrakt uprawnień IOS: `docs/architecture/ios-authority-contract.md`.

Dziennik jest warstwą audytu tych zasad, a nie alternatywnym źródłem autorytetu. W przypadku sprzeczności obowiązuje kontrakt uprawnień i polityka egzekwowana przez jądro.


## 14. Potwierdzenia komunikatów i prezentacja zdarzeń

Dziennik rozróżnia **potwierdzenie zapoznania się z komunikatem** od **zgody na operację**. Są to dwa różne typy zdarzeń i nie mogą być ze sobą utożsamiane.

Jeżeli użytkownik naciśnie przycisk komunikatu („Zrozumiałem”, „Przyjąłem do wiadomości” albo „Dobrze”), dziennik może zapisać:
- identyfikator komunikatu i powiązanego zdarzenia;
- fakt zapoznania się z komunikatem;
- czas potwierdzenia, jeśli zegar jest wiarygodny;
- wybraną etykietę przycisku.

Wpis taki nie jest zgodą, nie zmienia decyzji polityki i nie upoważnia do rozpoczęcia ani kontynuowania operacji. Brak potwierdzenia nie jest odmową ani zgodą. Zgody administratora nadal są rejestrowane odrębnym typem zdarzenia, z zakresem i uwierzytelnioną tożsamością zatwierdzającego zgodnie z sekcją 10.

Dziennik przechowuje fakty i wyniki; kolor jest wyłącznie warstwą prezentacji interfejsu. Widok codzienny może oznaczać żółtym zwykłe awarie i ostrzeżenia, a czerwonym awarie krytyczne oraz wiarygodnie potwierdzone zagrożenia. Kolor nie zmienia klasy uprawnień ani decyzji polityki. Komunikat o naprawie lub usunięciu jest dopuszczalny dopiero po potwierdzeniu rezultatu.
