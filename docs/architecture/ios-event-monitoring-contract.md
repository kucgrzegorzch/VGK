# Kontrakt monitorowania zdarzeń IOS

**Status:** propozycja architektoniczna P0.1. Dokument określa granice i przepływ przyszłego mechanizmu monitorowania. Nie oznacza, że IOS, kolektor zdarzeń ani trwały Dziennik Decyzji IOS są już zaimplementowane.

## 1. Cel i zakres

Pierwszym wdrażanym obszarem Inteligentnego Opiekuna Systemu (IOS) jest pasywne monitorowanie i rejestrowanie zdarzeń. IOS ma najpierw uzyskać wiarygodny obraz tego, co dzieje się w VGK, zanim otrzyma możliwość wykonywania działań ochronnych lub naprawczych.

P0.1 definiuje:
- źródła i format obserwacji;
- przepływ zdarzenia od źródła do kolektora;
- walidację, klasyfikację i ograniczenia zasobów;
- przekazywanie poprawnych zdarzeń do wspólnego Dziennika Decyzji IOS;
- zachowanie przy utracie danych, błędzie źródła lub niedostępności dziennika;
- kryteria testów i kolejność późniejszego wdrażania.

P0.1 nie implementuje usługi systemowej, sterowników, trwałego magazynu ani interfejsu użytkownika. Jest kontraktem, na podstawie którego można bezpiecznie przygotować mały, testowalny prototyp.

## 2. Zasady nadrzędne

1. **Obserwacja bez ingerencji:** monitor nie zatrzymuje procesów, nie blokuje plików, nie zmienia ustawień, nie naprawia systemu i nie wykonuje operacji uprzywilejowanych.
2. **Oddzielenie obserwacji od decyzji:** zgłoszenie źródła nie jest decyzją Silnika Polityki ani poleceniem wykonania działania.
3. **Jawna niepewność:** brak danych, nieznany czas, nieaktywne źródło i utracone zdarzenia nie mogą być przedstawiane jako stan prawidłowy.
4. **Ograniczone zasoby:** pamięć, rozmiar zdarzenia, liczba zdarzeń oczekujących i koszt przetwarzania mają jawne limity.
5. **Bezpieczne dane wejściowe:** treść zdarzenia jest danymi, nigdy instrukcją dla IOS. Pola tekstowe pochodzące z urządzeń, plików lub aplikacji nie mogą być wykonywane ani interpretowane jako polecenia.
6. **Prywatność:** zapis obejmuje minimum informacji potrzebnych do diagnozy; nie kopiuje treści prywatnych plików.
7. **Brak fałszywego sukcesu:** brak zdarzenia błędu nie dowodzi, że wszystkie elementy działają poprawnie.
8. **Niezależność ochrony krytycznej:** przyszłe zabezpieczenia sprzętowe i jądra nie mogą zależeć od działania interfejsu IOS ani od dostępności dziennika.

## 3. Rozdzielenie odpowiedzialności

Przepływ logiczny:

`Źródło` → `ograniczony kanał zdarzeń` → `Kolektor IOS` → `walidacja i klasyfikacja` → `Dziennik Decyzji IOS` → `odczyt historii`

### Źródło zdarzenia

Źródło zgłasza wyłącznie fakt, który może zaobserwować, np. kod błędu, zmianę stanu albo zakończenie kontroli. Nie przyznaje sobie uprawnień, nie określa działań ochronnych i nie zapisuje bezpośrednio do magazynu dziennika.

### Kanał zdarzeń

Kanał przekazuje ograniczone komunikaty. Nie może przyjmować nieograniczonych buforów ani powodować blokowania krytycznej ścieżki systemu. Przepełnienie jest wykrywalne i raportowane.

### Kolektor IOS

Kolektor:
- przyjmuje zdarzenia tylko od zarejestrowanych źródeł;
- sprawdza wersję, rozmiar, typ i zakres pól;
- odrzuca błędne lub niezgodne komunikaty;
- przypisuje własny numer kolejny i identyfikator przyjętego wpisu;
- zachowuje informację o niepewności i utracie danych;
- przekazuje poprawny rekord do dziennika.

Kolektor nie uruchamia napraw, nie zmienia polityki i nie wydaje poleceń wykonawczych.

### Dziennik Decyzji IOS – wspólny dziennik audytowy

W VGK nie projektujemy dwóch niezależnych magazynów: osobnego Dziennika Zdarzeń i osobnego Dziennika Decyzji. Istniejący **Dziennik Decyzji IOS** jest jednym, wspólnym dziennikiem audytowym. Obejmuje zarówno obserwacje i wyniki monitorowania, jak i późniejsze walidacje, decyzje, zgody, wykonania oraz weryfikacje. Określenie „zdarzenia IOS” opisuje zawartość i strumień wejściowy, a nie drugi plik, bazę ani niezależny mechanizm zapisu.

Kolektor przekazuje zatwierdzone rekordy do wspólnego dziennika zgodnie z `ios-decision-audit-log.md`. Ten dokument określa wyłącznie warstwę przyjmowania i przygotowania zdarzeń; nie tworzy alternatywnego formatu audytu ani nie zmienia zasad zapisu decyzji przed wykonaniem operacji.

### Odbiorcy odczytu

Historia interfejsu i narzędzia serwisowe mogą odczytywać zapisane zdarzenia. Nie mogą zmieniać źródłowych rekordów ani nadawać zdarzeniom wyższej wiarygodności.

## 4. Źródła zdarzeń i ich rejestracja

Źródło musi mieć jawny identyfikator, wersję kontraktu i określony zakres obserwacji. Kolektor nie może automatycznie ufać każdemu modułowi, który próbuje zgłosić zdarzenie. Samo wpisanie `source_id` do komunikatu nie uwierzytelnia źródła.

Tożsamość źródła musi wynikać z kontrolowanego kanału komunikacji lub rejestru źródeł utrzymywanego przez zaufaną część systemu, a nie z deklaracji nadawcy. Rejestr przypisuje źródłu dozwolone kategorie, kody zdarzeń, maksymalną ważność, limity częstotliwości i zakres danych. Kolektor odrzuca próbę podszycia się pod inne źródło oraz zdarzenie spoza przyznanego katalogu. Jeżeli obecna platforma nie potrafi jeszcze wiarygodnie uwierzytelnić źródła, rekord musi zostać oznaczony jako niezaufany/niezweryfikowany i nie może stanowić podstawy automatycznej decyzji ochronnej.

Docelowe kategorie źródeł:
- jądro i start systemu;
- procesor i funkcje platformy;
- pamięć;
- urządzenia i sterowniki;
- magazyn danych i system plików;
- integralność oraz bezpieczeństwo;
- usługi systemowe;
- diagnostyka i odzyskiwanie.

Lista kategorii nie oznacza, że odpowiednie źródła już istnieją. Źródło można aktywować dopiero wtedy, gdy istnieje rzeczywisty, testowalny mechanizm pozyskania informacji.

Każde źródło powinno określać:
- stabilny `source_id` i `source_version`;
- obsługiwane typy zdarzeń;
- maksymalny rozmiar komunikatu;
- ograniczenie częstotliwości;
- sposób wykrywania niedostępności;
- pola, których źródło nie potrafi wiarygodnie ustalić.

Źródło nie może samodzielnie podwyższyć swojej klasy ważności ani wybrać dowolnego poziomu pewności. Dozwolone wartości są ograniczone rejestrem źródeł i katalogiem kodów. Źródło nie może samodzielnie oznaczać pliku jako potwierdzonego malware ani przedstawiać podejrzenia jako faktu. Może zgłosić obserwację wraz z kodem i poziomem pewności mieszczącym się w przyznanym zakresie; kolektor sprawdza tę zgodność.

## 5. Minimalny kontrakt zdarzenia wejściowego

Każde zgłoszenie powinno zawierać lub jednoznacznie mapować następujące pola:

| Pole | Znaczenie |
|---|---|
| `schema_version` | wersja formatu komunikatu |
| `source_id` | identyfikator zarejestrowanego źródła |
| `source_version` | wersja źródła |
| `source_sequence` | numer źródłowy, jeśli źródło potrafi go wiarygodnie prowadzić |
| `event_code` | stabilny kod z katalogu danego źródła |
| `category` | zamknięta kategoria zdarzenia |
| `severity` | ważność: info, warning, error lub critical |
| `confidence` | pewność: confirmed, suspected lub unknown |
| `monotonic_time` | czas monotoniczny, jeśli jest dostępny |
| `time_status` | trusted, unavailable lub uncertain |
| `subject_ref` | ograniczony identyfikator zasobu, jeśli dotyczy |
| `correlation_id` | powiązanie z innymi zdarzeniami, jeśli dotyczy |
| `payload` | małe, typowane dane właściwe dla kodu zdarzenia |

Kolektor uzupełnia rekord o pola wspólnego schematu Dziennika Decyzji: `event_id`, rosnące `sequence`, `boot_session_id`, `event_type` oraz `timestamp` i `monotonic_time` zgodnie z ich dostępnością. Nie wprowadzamy osobnego pola `journal_sequence` – obowiązuje kanoniczne pole `sequence` z `ios-decision-audit-log.md`. `event_type` określa etap audytu (np. `observation` lub `validation`), natomiast `event_code` jest stabilnym kodem konkretnego rodzaju obserwacji. `category`, `severity` i `confidence` są dodatkowymi polami opisującymi zdarzenie. `correlation_id` i `source` zachowują znaczenie z istniejącego schematu. Brakującego czasu nie wolno zastępować wymyśloną datą.

Wartości tekstowe i identyfikatory mają limity długości. Nieznane pola nie mogą zmieniać uprawnień ani zachowania systemu. Niezgodna wersja schematu jest odrzucana lub kierowana do jawnego stanu nieobsługiwanej wersji; nie może być interpretowana częściowo jako zaufane polecenie.

## 6. Ważność, pewność i prezentacja

Ważność i pewność są oddzielnymi wymiarami:
- `severity` mówi, jak poważny jest skutek, jeśli obserwacja jest prawdziwa;
- `confidence` mówi, jak mocne są dowody tej obserwacji.

Przykładowo zdarzenie może mieć ważność `critical`, ale pewność `unknown`. Nie wolno wtedy przedstawiać go jako potwierdzonego zagrożenia.

Kolory żółty i czerwony należą do warstwy prezentacji opisanej w kontrakcie interfejsu. Nie są wartościami autoryzacji, nie zmieniają polityki i nie zastępują tekstowej klasyfikacji. Dziennik przechowuje faktyczne kody ważności i pewności.

## 7. Kolejność, duplikaty i utrata zdarzeń

- Kolektor nadaje rosnące `sequence` zgodnie ze wspólnym schematem Dziennika Decyzji; nie prowadzi równoległej numeracji dziennika zdarzeń.
- `source_sequence`, jeśli istnieje, pomaga wykrywać luki lub duplikaty po stronie danego źródła.
- `monotonic_time` może porządkować zdarzenia w obrębie jednej sesji startowej; czas ścienny służy wyłącznie prezentacji i może być niepewny.
- Powtórzony komunikat nie może po cichu tworzyć fałszywego nowego przypadku. Reguła deduplikacji musi uwzględniać źródło, sesję i kod zdarzenia; oryginalne obserwacje nie mogą być bezpowrotnie gubione.
- Utrata komunikatów musi być sygnalizowana licznikiem lub specjalnym rekordem `event_loss`, gdy tylko możliwe będzie jego bezpieczne zapisanie.
- Kolektor nie może twierdzić, że historia jest kompletna, jeżeli źródło zgłosiło lukę, kanał się przepełnił albo dziennik był niedostępny.

Dla ograniczenia zasobów kanał wejściowy ma stałą, określoną pojemność. Zdarzenia krytyczne mogą korzystać z małej, zarezerwowanej puli, ale rezerwa również musi mieć limit. Nie wolno tworzyć nieograniczonej kolejki ani blokować jądra w oczekiwaniu na zapis.

## 8. Dziennik i trwałość zapisu

Kontrakt trwałego zapisu jest doprecyzowaniem, a nie zastępstwem `ios-decision-audit-log.md`. Istnieje jeden wspólny dziennik i jedna kanoniczna numeracja.

Wymagania:
- rekordy są dopisywane; zwykły odbiorca nie może ich edytować ani usuwać;
- każdy rekord ma wersję, długość i kontrolę poprawności formatu;
- częściowy lub ucięty zapis jest wykrywalny;
- po restarcie można odróżnić ostatni kompletny rekord od niekompletnego końca pliku;
- limity rozmiaru, rotacja i rezerwa na zdarzenia krytyczne są jawne;
- brak miejsca lub błąd zapisu nie może być przedstawiany jako udany zapis;
- brak zapisu nie może automatycznie zezwalać na przyszłe operacje, które wymagają audytu.

Na wczesnym etapie prototypu dopuszczalny jest wyłącznie testowy odbiornik pamięciowy, aby sprawdzić kontrakt bez udawania trwałości. Nie jest on Dziennikiem Zdarzeń produkcyjnego systemu i nie zapewnia zachowania danych po restarcie.

Łańcuch skrótów, podpisywanie punktów kontrolnych, ochrona kluczy i wykrywanie cofnięcia nośnika pozostają osobnym etapem. Dopóki nie zostaną zaprojektowane i przetestowane, nie wolno deklarować odporności dziennika na celową manipulację.

## 9. Zachowanie przy błędach

| Problem | Wymagane zachowanie |
|---|---|
| Źródło nie odpowiada | oznaczyć źródło jako niedostępne lub stan nieznany; nie zakładać poprawności |
| Błędny format | odrzucić komunikat, zwiększyć licznik błędów źródła |
| Przepełnienie kanału | zachować ograniczone zużycie zasobów i zgłosić utratę |
| Dziennik niedostępny | zgłosić ograniczenie rejestrowania; nie udawać zapisu |
| Niepewny zegar | zachować porządek sekwencji i jawnie oznaczyć czas |
| Brak pamięci | nie alokować bez ograniczeń; stosować ustaloną politykę odrzucania |
| Nieznana wersja schematu | nie interpretować nieznanych pól; jawnie oznaczyć niezgodność |
| Restart podczas zapisu | odzyskać wyłącznie kompletne rekordy i zgłosić niekompletny koniec |

Awaria monitora lub dziennika nie może zatrzymać niezależnych mechanizmów sprzętowych, ochrony termicznej ani podstawowego działania jądra. Jednocześnie interfejs nie może pokazywać stanu „wszystko prawidłowo”, gdy monitorowanie jest nieaktywne lub niekompletne.

## 10. Granice P0.1

W tym etapie zabronione jest:
- zatrzymywanie, uruchamianie lub kończenie procesów;
- blokowanie, kwarantanna, przenoszenie lub usuwanie plików;
- zmiana uprawnień, konfiguracji, kluczy lub polityki;
- ładowanie albo odłączanie sterowników;
- naprawianie systemu plików lub pamięci;
- restart lub wyłączanie komputera z poziomu monitora;
- podejmowanie decyzji bezpieczeństwa wyłącznie na podstawie pojedynczej obserwacji.

Monitor może zgłosić fakt i jego niepewność. Ocena oraz ewentualne działanie należą do późniejszych, odrębnie zatwierdzonych etapów IOS.

## 11. Etapy wdrożenia po P0.1

| Etap | Zakres | Warunek przejścia |
|---|---|---|
| P0.1 | niniejszy kontrakt architektoniczny | przegląd spójności z architekturą IOS, uprawnieniami i dziennikiem |
| P0.2 | mały prototyp kolektora w środowisku testowym, ze stałą kolejką i odbiornikiem pamięciowym | testy formatów, limitów, błędów, duplikatów i utraty |
| P0.3 | adapter trwałego zapisu po przygotowaniu odpowiedniego, testowalnego mechanizmu przechowywania | testy częściowego zapisu, restartu, rotacji i odzyskiwania |
| P0.4 | podłączenie pierwszych rzeczywistych, wyłącznie odczytowych źródeł VGK | każde źródło ma własne testy i udokumentowane ograniczenia |
| P0.5 | odczyt historii przez prosty interfejs IOS | interfejs nie może modyfikować dziennika ani sterować ochroną |

Kolejność może zostać zmieniona wyłącznie po udokumentowaniu zależności technicznych. Nie należy implementować trwałego dziennika, dopóki VGK nie ma bezpiecznego i testowalnego mechanizmu przechowywania danych.

## 12. Kryteria akceptacji P0.1

- Dokument jednoznacznie oddziela źródła, kanał, kolektor, wspólny Dziennik Decyzji i odbiorców odczytu.
- Nie powstaje drugi, niezależny Dziennik Decyzji IOS ani równoległy schemat numeracji.
- Obserwacja nie jest decyzją polityki ani poleceniem wykonawczym.
- Ważność jest odróżniona od pewności.
- Format wejściowy, limity, wersjonowanie i sposób uwierzytelniania źródeł są określone.
- Nadawca nie może podszyć się pod inne źródło ani samodzielnie podwyższyć ważności zdarzenia.
- Przepełnienie, utrata, błędny komunikat i niedostępność dziennika mają jawne zachowanie.
- Nie ma obietnicy trwałości ani odporności kryptograficznej bez implementacji i testów.
- Nie ma automatycznych napraw ani zmian stanu systemu.
- Dokument jest zgodny z kontraktem uprawnień, Dziennikiem Decyzji i uproszczonym interfejsem IOS.
- Stan dokumentu jest opisany jako projekt, a nie działająca funkcja.

## 13. Powiązane dokumenty

- Architektura IOS: `docs/architecture/intelligent-system-caretaker.md`.
- Kontrakt uprawnień IOS: `docs/architecture/ios-authority-contract.md`.
- Kontrakt interfejsu IOS: `docs/architecture/ios-user-interface-contract.md`.
- Model zagrożeń VGK: `docs/architecture/threat-model.md`.

W przypadku sprzeczności obowiązują granice uprawnień i polityka bezpieczeństwa VGK. Niniejszy dokument nie przyznaje IOS nowych uprawnień i nie upoważnia do aktywowania żadnej operacji uprzywilejowanej.
