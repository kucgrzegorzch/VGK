# Architektura VGK Guardian Security

**Status:** propozycja architektoniczna, zależna od modelu zagrożeń VGK. Nie opisuje gotowego ani aktywnego podsystemu ochrony.

**Zależność przeglądu:** dokument rozwija model zagrożeń z gałęzi `design/vgk-threat-model` (PR #81). Ten PR powinien być oceniany po PR #81, a nie niezależnie od niego.

## 1. Cel i podstawowa decyzja architektoniczna

VGK Guardian Security ma być nadrzędnym, kontrolowanym systemem ochrony, a nie pojedynczym antywirusem. Jego zadaniem będzie koordynowanie weryfikacji, polityk uprawnień, inspekcji, reakcji i odzyskiwania.

Najważniejsza zasada:

**Komponent, który wykrywa zagrożenie, nie otrzymuje automatycznie prawa do wykonania dowolnej operacji naprawczej.**

Skaner może zgłosić podejrzenie. Mechanizm polityki ocenia je według reguł. Osobny autorytet VGK zatwierdza dozwoloną operację. Wykonawca realizuje wyłącznie zatwierdzoną czynność w ograniczonym zakresie.

Podsystem musi:
- chronić integralność jądra i granicę uprawnień;
- działać lokalnie w podstawowym zakresie;
- obsługiwać zgodne narzędzia zewnętrzne bez oddawania im kontroli nad jądrem;
- mieć ograniczone zużycie pamięci i CPU;
- rejestrować ważne decyzje;
- umożliwiać bezpieczne zatrzymanie operacji i odzyskiwanie;
- jasno odróżniać decyzje automatyczne od czynności wymagających zgody użytkownika.

## 2. Podział na komponenty

Poniższy podział jest logiczny. Nie przesądza jeszcze, czy każdy komponent będzie osobnym procesem, biblioteką czy częścią jądra. Granice uprawnień są ważniejsze niż granice plików.

| Komponent | Odpowiedzialność | Uprawnienia docelowe |
|---|---|---|
| **Guardian Core** | koordynuje zdarzenia, kolejność kontroli i stan podsystemu | ograniczone uprawnienia koordynacyjne; bez dowolnego dostępu do danych użytkownika |
| **Trust Verifier** | sprawdza podpisy, wykazy, skróty, wydawcę i wersję | odczyt kontrolowanych metadanych i plików; bez prawa do samodzielnego usuwania |
| **Policy Engine** | ocenia wynik weryfikacji i reguły bezpieczeństwa | odczyt polityk i kontekstu; wydaje decyzję, ale nie modyfikuje bezpośrednio plików |
| **Execution Gate** | egzekwuje decyzję przed uruchomieniem programu lub załadowaniem sterownika | uprzywilejowany, wąski interfejs systemowy; nie wykonuje skanowania |
| **Inspection Service** | skanuje pliki, nośniki i wybrane zdarzenia | ograniczony odczyt; bez dostępu do pamięci jądra |
| **Quarantine Manager** | przenosi podejrzane obiekty do kontrolowanej kwarantanny i obsługuje przywracanie | dostęp tylko do własnego obszaru kwarantanny i zatwierdzonych operacji |
| **Event Recorder** | zapisuje decyzje i istotne zdarzenia ochronne | dopisywanie zdarzeń; brak swobodnego kasowania wcześniejszych zapisów |
| **Recovery Coordinator** | uruchamia sprawdzone procedury odzyskiwania | wykonuje wyłącznie zatwierdzone i zweryfikowane procedury |
| **External Tool Adapter** | udostępnia ograniczone API zgodnym skanerom zewnętrznym | uprawnienia jawnie przyznane, ograniczone do konkretnej operacji |

### 2.1 Guardian Core nie jest superużytkownikiem wszystkiego

Guardian Core koordynuje pracę, ale nie powinien mieć automatycznie pełnego dostępu do każdego zasobu. W szczególności:
- nie przechowuje klucza prywatnego wydawcy;
- nie może dowolnie zapisywać do pamięci jądra;
- nie może samodzielnie wyłączać Execution Gate;
- nie może usuwać dziennika zdarzeń;
- nie może wykonywać trwałych operacji na danych użytkownika bez właściwej autoryzacji.

Jeżeli Guardian Core ulegnie awarii, podstawowe mechanizmy ochrony jądra i kontroli uprawnień nie mogą zostać automatycznie wyłączone.

## 3. Trzy oddzielne płaszczyzny

Architekturę należy podzielić na trzy płaszczyzny, aby wykrywanie, decyzje i wykonanie nie były skupione w jednym miejscu.

### A. Płaszczyzna wykrywania
- Trust Verifier;
- Inspection Service;
- źródła zdarzeń jądra i systemu plików;
- adaptery zgodnych narzędzi zewnętrznych.

Ta płaszczyzna dostarcza wyniki i dowody. Nie może samodzielnie nadawać sobie uprawnień.

### B. Płaszczyzna decyzji
- Policy Engine;
- kontekst użytkownika, procesu i zasobu;
- reguły zaufania, integralności i uprawnień.

Wynikiem jest jawna decyzja z określonym zakresem, przyczyną, czasem ważności i wymaganym poziomem zgody.

### C. Płaszczyzna egzekwowania
- Execution Gate;
- Quarantine Manager;
- kontrolowane operacje sieciowe;
- Recovery Coordinator.

Wykonuje tylko operacje, które są dozwolone przez politykę i mieszczą się w przyznanym zakresie.

## 4. Przepływ decyzji

Docelowy przepływ dla podejrzanego pliku:

1. System otrzymuje zdarzenie, np. próbę uruchomienia pliku.
2. Trust Verifier sprawdza podpis, wykaz, skrót i status zaufania.
3. Inspection Service może przeprowadzić dodatkową inspekcję, jeśli jest wymagana przez politykę.
4. Policy Engine ocenia wyniki, kontekst i poziom ryzyka.
5. Guardian Core przekazuje decyzję do właściwego mechanizmu egzekwowania.
6. Execution Gate zezwala na uruchomienie, odmawia go lub wymaga dodatkowego zatwierdzenia.
7. Event Recorder zapisuje wynik.
8. Jeżeli potrzebna jest kwarantanna lub naprawa, Quarantine Manager albo Recovery Coordinator wykonuje tylko osobno zatwierdzoną operację.

Ważne: błąd skanera nie jest dowodem, że plik jest złośliwy. Jednak błąd krytycznej weryfikacji podpisu składnika systemowego nie może zostać zignorowany tylko dlatego, że skaner nie zgłosił zagrożenia.

## 5. Kontrakt decyzji bezpieczeństwa

Każda decyzja powinna mieć co najmniej:
- identyfikator zdarzenia;
- identyfikator procesu lub obiektu, którego dotyczy;
- rodzaj operacji;
- zasób i zakres operacji;
- źródło oraz wynik weryfikacji;
- zastosowaną regułę polityki;
- decyzję: `ALLOW`, `DENY`, `REQUIRE_APPROVAL` lub `ISOLATE`;
- poziom pewności i powód;
- czas ważności decyzji;
- informację, czy decyzja została wykonana i z jakim wynikiem.

Decyzja musi być związana z konkretnym obiektem i operacją. Nie może być ogólnym, bezterminowym zezwoleniem dla programu na wszystkie przyszłe działania.

### 5.1 Rozdzielenie błędu od zagrożenia

System powinien rozróżniać:
- **potwierdzona niezgodność integralności** – np. podpis lub skrót nie pasuje;
- **podejrzenie** – heurystyka lub skaner wykrył cechę ryzykowną;
- **błąd techniczny** – skaner nie uruchomił się, zabrakło pamięci lub odczyt się nie powiódł;
- **brak informacji** – nie ma dostępnej reputacji lub połączenia z siecią.

Tych stanów nie wolno sprowadzać do jednego wyniku „wirus”. Krytyczna niezgodność zaufania systemowego oznacza odmowę użycia danego składnika. Błąd dodatkowego skanera powinien być obsłużony według jawnej polityki ryzyka, bez automatycznego kasowania pliku.

## 6. Poziomy reakcji

| Poziom | Przykładowa reakcja | Zasada |
|---|---|---|
| 0 – informacja | zapis i komunikat | bez ingerencji w zasób |
| 1 – odmowa operacji | nie uruchamiaj pliku lub nie ładuj sterownika | automatycznie, gdy wymagany warunek zaufania nie jest spełniony |
| 2 – ograniczenie | zablokuj konkretną operację lub połączenie | zakres i czas ograniczone, możliwe do cofnięcia |
| 3 – izolacja | odłącz proces od wskazanego zasobu lub przenieś obiekt do kwarantanny | tylko przez kontrolowany mechanizm, z możliwością odtworzenia |
| 4 – naprawa | odtwórz zweryfikowany plik lub cofnij zmianę | wymaga sprawdzonej procedury; operacje na danych użytkownika wymagają zgody |
| 5 – odzyskiwanie | uruchom SafeCore / tryb awaryjny | gdy normalne środowisko nie może bezpiecznie działać |

Poziom wyższy nie powinien być uruchamiany automatycznie tylko dlatego, że poprzedni nie rozwiązał problemu. Każda eskalacja wymaga własnych warunków i kontroli.

## 7. Kwarantanna – wymagania bezpieczeństwa

Kwarantanna nie może być zwykłym katalogiem, do którego każdy proces może zapisywać.

Docelowe wymagania:
- dostęp wyłącznie przez Quarantine Manager;
- zachowanie oryginalnego skrótu, źródła i czasu wykrycia;
- zapis powodu i decyzji, która doprowadziła do izolacji;
- brak automatycznego wykonywania obiektu w kwarantannie;
- kontrola miejsca i limitów zajętości;
- możliwość przywrócenia po ponownej weryfikacji i autoryzacji;
- ochrona przed podmianą obiektu między skanowaniem a kwarantanną;
- czytelna informacja dla użytkownika.

Kwarantanna nie powinna bez potrzeby kopiować wielokrotnie dużych plików. Preferowana metoda przechowywania zależy od przyszłego systemu plików i musi być zaprojektowana z uwzględnieniem awarii zasilania.

## 8. Zewnętrzne skanery i API

Zgodnie z polityką zgodności VGK może dopuścić skanery działające w przestrzeni użytkownika. Adapter zewnętrznych narzędzi powinien:
- uwierzytelniać aplikację i sprawdzać przyznane uprawnienia;
- udostępniać wyłącznie wersjonowane, udokumentowane operacje;
- ograniczać rozmiar żądań, czas działania i liczbę równoległych zadań;
- traktować wynik skanera jako raport, a nie jako polecenie administracyjne;
- nie udostępniać pamięci jądra, rejestrów CPU ani nieograniczonego dostępu do procesów;
- nie umożliwiać cichego wyłączenia ochrony systemowej.

Proponowane minimalne operacje API:
- sprawdź wskazany plik w zakresie przyznanych uprawnień;
- pobierz ograniczony zestaw zdarzeń bezpieczeństwa;
- zgłoś podejrzenie dla konkretnego obiektu;
- poproś o kwarantannę konkretnego obiektu;
- pobierz wynik wykonanej decyzji.

API nie powinno zawierać operacji „wyłącz wszystkie zabezpieczenia” ani „wykonaj dowolny kod jako jądro”.

## 9. Zachowanie przy awarii

| Awaria | Wymagane zachowanie |
|---|---|
| Inspection Service nie działa | nie uznawaj braku wyniku za czysty wynik; zastosuj politykę dla danego rodzaju pliku i operacji |
| Policy Engine nie odpowiada | odmów nowych operacji wysokiego ryzyka; nie wyłączaj istniejących ochron jądra |
| Guardian Core uległ awarii | izolowane mechanizmy egzekwowania zachowują ostatnią bezpieczną politykę; zgłoś awarię |
| Event Recorder nie może zapisać | krytyczna operacja może zostać wstrzymana, jeśli audyt jest warunkiem bezpieczeństwa; nie kasuj wcześniejszych zdarzeń |
| Brak pamięci | ogranicz lub wstrzymaj skanowanie; nie destabilizuj jądra |
| Brak Internetu | działaj na lokalnych kluczach, politykach i danych; oznacz brak aktualnej reputacji sieciowej |
| Kwarantanna jest pełna | nie usuwaj po cichu starszych obiektów; odmów nowej kwarantanny i poinformuj o ograniczeniu |
| Nieudana naprawa | przerwij, zachowaj materiał diagnostyczny i przejdź do kontrolowanego odzyskiwania |

Dla operacji krytycznych obowiązuje zasada fail-closed: brak potwierdzenia zaufania nie daje zgody na uruchomienie. Dla usług dodatkowych, takich jak heurystyczny skaner, awaria nie może powodować nieuzasadnionego zatrzymania całego systemu. Polityka musi rozróżniać te klasy.

## 10. Wymagania wydajnościowe

Docelowy system ma działać także na komputerach z 4–8 GB RAM:
- skanowanie porcjami z twardymi limitami pamięci;
- kolejka zadań z ograniczeniem współbieżności;
- priorytet dla operacji interaktywnych i krytycznych;
- możliwość odroczenia pełnego skanowania;
- kontrola czasu CPU i I/O;
- unikanie przechowywania całych plików w RAM, gdy wystarczy strumieniowe przetwarzanie;
- brak nieograniczonego buforowania zdarzeń;
- odporność na pliki o skrajnych rozmiarach i zagnieżdżone archiwa.

Limity muszą być mierzone na rzeczywistym sprzęcie. Nie należy wpisywać arbitralnych wartości jako gwarantowanych, dopóki nie zostaną sprawdzone w testach.

## 11. Etapy implementacji i zależności

1. **Kontrakty i model uprawnień** – typy zdarzeń, decyzji, identyfikatory obiektów, granice API. Bez zmian w jądrowym egzekwowaniu.
2. **Trust Verifier** – wykorzystanie i domknięcie istniejących narzędzi kryptograficznych oraz weryfikacji VGKM; testy podpisu, manifestu i plików.
3. **Execution Gate** – dopiero po ustaleniu bezpiecznego punktu integracji ze startem i mechanizmem ładowania; odmowa dla składników systemowych, których zaufania nie potwierdzono.
4. **Event Recorder i Policy Engine** – reguły, raportowanie, testy błędów i nadużyć.
5. **Inspection Service** – skanowanie plików i nośników w przestrzeni użytkownika z limitami zasobów.
6. **Quarantine Manager** – odwracalna izolacja, ochrona przed wyścigami i testy awarii zasilania.
7. **External Tool Adapter** – wersjonowane API i testy, że skaner nie może eskalować uprawnień.
8. **Recovery Coordinator** – integracja z SafeCore, dopiero po opracowaniu bezpiecznego modelu kopii i odtwarzania.

Każdy etap ma osobny zakres, testy i przegląd. Nie należy implementować wszystkich komponentów naraz ani aktywować nieprzetestowanego egzekwowania w zwykłym starcie.

## 12. Kryteria akceptacji architektury

Przed implementacją należy potwierdzić:
- brak pojedynczego komponentu, który jednocześnie wykrywa, decyduje i może dowolnie modyfikować system;
- brak ścieżki, w której skaner user-space uzyskuje dostęp do pamięci jądra;
- awaria skanera nie wyłącza weryfikacji składników systemowych;
- brak podpisu lub niezgodność integralności krytycznego składnika powoduje odmowę użycia;
- decyzje są związane z konkretnym obiektem i operacją;
- kwarantanna nie usuwa automatycznie danych;
- awaria dziennika i brak pamięci mają zdefiniowane zachowanie;
- API ma limity i testy nadużyć;
- testy obejmują fałszywe alarmy, brak sieci, pełną kwarantannę i awarię komponentu;
- raporty jasno oddzielają projekt, implementację i potwierdzone testy.

## 13. Stan implementacji

Na podstawie dokumentacji projektu:
- istnieją moduły kryptograficzne i parser VGKM oraz testy pomocniczej weryfikacji wydania;
- ich obecność nie oznacza aktywnej weryfikacji w ścieżce uruchamiania systemu;
- opisane komponenty Guardian Security, Policy Engine, Execution Gate, kwarantanna, API skanerów i pełny Recovery Coordinator są **projektowane**, a nie gotowe;
- nie ma podstaw, by określać obecny VGK jako system z działającym antywirusem.

Każdy status należy ponownie sprawdzić w kodzie i testach przed zmianą tego opisu.
