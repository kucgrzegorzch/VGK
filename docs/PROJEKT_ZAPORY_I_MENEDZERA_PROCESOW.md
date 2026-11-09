# Projekt VGK: zapora sieciowa i menedżer procesów

Status: projekt funkcjonalny do omówienia i dalszego etapowego wdrożenia.
Gałąź: feature/vgk-product-foundation

## 1. Założenia nadrzędne

- Zapora jest domyślnie włączona po instalacji VGK.
- Ustawienia mają być zrozumiałe dla zwykłego użytkownika; szczegóły techniczne są dostępne po rozwinięciu.
- System nie może mylić połączenia niezbędnego z połączeniem zaufanym. Czerwony kolor oznacza ryzyko przerwania ważnej funkcji, a nie wykryte zagrożenie.
- Nieznane połączenia otrzymują stan „do sprawdzenia”, a nie automatycznie etykietę zagrożenia.
- Każda reguła ma opis, zakres, źródło decyzji i możliwość cofnięcia.
- Interfejs musi być dostępny także z klawiatury i ekranu dotykowego; podpowiedź nie może zależeć wyłącznie od najechania myszą.

## 2. Interfejs zapory

Główne okno pokazuje:
- stan zapory i profil sieci;
- aktywne połączenia przychodzące i wychodzące;
- nazwę procesu/aplikacji, ścieżkę pliku, adres lokalny i zdalny, port, protokół, kierunek i czas trwania;
- przyczynę dopuszczenia lub zablokowania oraz regułę, która zadecydowała;
- liczbę przesłanych i odebranych danych, jeśli sterownik sieciowy udostępnia te informacje.

### Znaczenie kolorów

- Czerwony: połączenie lub usługa oznaczona jako istotna; blokada może przerwać sieć, aktualizacje, rozwiązywanie nazw lub działanie systemu. To nie jest automatycznie oznaczenie złośliwego ruchu.
- Zielony: połączenie rozpoznane i niekrytyczne; można rozważyć jego zablokowanie. Zielony nie jest gwarancją bezpieczeństwa.
- Bursztynowy: połączenie nieznane, nowe lub wymagające decyzji.
- Szary: połączenie nieaktywne albo tylko informacyjne.

Kolor zawsze występuje razem z tekstowym opisem i ikoną; nie może być jedynym nośnikiem znaczenia.

### Informacja przed blokadą

Po wybraniu połączenia (myszą, dotykiem lub klawiaturą) panel szczegółów wyjaśnia:
- co zapewnia dane połączenie;
- jakie funkcje mogą przestać działać po blokadzie;
- czy blokada dotyczy jednego połączenia, aplikacji, adresu IP, portu czy całej usługi;
- jak cofnąć decyzję.

Jeśli VGK nie potrafi przewidzieć skutków, ma powiedzieć „Skutek nieznany — blokada może przerwać działanie aplikacji lub usługi”, a nie zgadywać.

## 3. Biała i czarna lista

Prawa część okna zawiera dwie zakładki/listy.

### Biała lista — zezwalaj

Użytkownik może dodać:
- konkretną aplikację lub plik wykonywalny;
- usługę systemową;
- adres IP lub zakres adresów;
- w późniejszym etapie domenę, jeśli system ma bezpieczny mechanizm wiązania nazwy z adresem.

Każdy wpis ma zakres: kierunek, protokół, port, profil sieci i ewentualny termin ważności. Wpis nie powinien nieświadomie otwierać wszystkich portów.

### Czarna lista — blokuj

Użytkownik może dodać:
- adres IP lub zakres adresów;
- aplikację;
- plik wykonywalny wskazany z dysku, w tym z partycji C: po jej zamontowaniu i rozpoznaniu przez VGK.

Sama ścieżka pliku nie wystarcza jako trwała tożsamość. VGK powinien zapamiętywać ścieżkę oraz, gdy będzie to możliwe, skrót kryptograficzny pliku i wydawcę/podpis. Po podmianie pliku system ma ponownie ocenić regułę i ostrzec użytkownika. Reguła dla pliku nie może oznaczać blokowania całego dysku ani wszystkich plików w katalogu, chyba że użytkownik wyraźnie wybierze taki zakres.

### Pierwszeństwo reguł

- Jawna blokada ma pierwszeństwo przed zezwoleniem, aby wpis na białej liście nie omijał czarnej listy.
- Konflikt reguł jest pokazywany użytkownikowi.
- Krytyczne usługi systemowe są chronione przed przypadkowym wyłączeniem; świadoma zmiana wymaga wyjaśnienia skutków i dodatkowego potwierdzenia.
- Użytkownik może cofnąć własną regułę i przywrócić ustawienia domyślne.

## 4. Zachowanie domyślne po instalacji

- Zapora jest aktywna od pierwszego uruchomienia.
- Nieoczekiwane połączenia przychodzące są domyślnie blokowane.
- Niezbędne mechanizmy uzyskania łączności (np. konfiguracja sieci, rozwiązywanie nazw i komunikacja potrzebna do aktualizacji) otrzymują wąskie, jawne reguły.
- Nowe aplikacje proszące o dostęp do sieci są przedstawiane z nazwą, ścieżką i wyjaśnieniem; użytkownik może zezwolić jednorazowo, na czas działania lub na stałe, albo odmówić.
- W trybie awaryjnym istnieje bezpieczna możliwość naprawienia błędnej reguły bez wyłączania całej ochrony na stałe.

## 5. Architektura zapory — etapowanie

1. Model reguł i testy jednostkowe: dopasowanie procesu, adresu, portu, protokołu i kierunku; rozstrzyganie konfliktów.
2. Usługa zapory działająca z ograniczonymi uprawnieniami oraz trwały, wersjonowany zapis reguł.
3. Integracja z warstwą sieciową jądra: filtrowanie ruchu przychodzącego i wychodzącego, bez polegania wyłącznie na interfejsie graficznym.
4. Interfejs pokazujący rzeczywiste połączenia i skutki reguł.
5. Testy awarii, restartu, aktualizacji, IPv4/IPv6, wielu kart sieciowych i trybu awaryjnego.

Zapora musi działać także wtedy, gdy interfejs graficzny jest zamknięty. Awaria panelu nie może wyłączać filtrowania. Zmiany reguł powinny być atomowe, a błędna konfiguracja nie może pozostawić systemu bez kontroli dostępu.

## 6. Menedżer procesów VGK

VGK powinien otrzymać własną aplikację systemową, a nie wymagać instalowania programu Windows.

Widok podstawowy:
- lista procesów i drzewo procesów (program nadrzędny i uruchomione przez niego elementy);
- obciążenie procesora, zajętość pamięci, użycie dysku i sieci;
- nazwa programu, użytkownik, czas działania i stan;
- wyszukiwanie, sortowanie i filtrowanie.

Dostępne działania:
- zakończ zadanie i — po ostrzeżeniu — wymuś zakończenie;
- zmień priorytet w bezpiecznym zakresie;
- pokaż lokalizację pliku i informacje o wydawcy/podpisie, jeśli dostępne;
- zarządzaj programami uruchamianymi wraz z systemem;
- pokaż usługi i ich zależności;
- później: przypisanie rdzeni procesora, limity zasobów i historia obciążenia.

Procesów jądra, zapory, mechanizmów bezpieczeństwa i krytycznych usług nie wolno kończyć zwykłym przyciskiem. Wymagane są uprawnienia administratora do działań wpływających na inne konta lub cały system.

## 7. Proces Lasso jako inspiracja

Process Lasso jest programem przeznaczonym dla Windows. Nie należy zakładać, że jego gotowy plik wykonywalny można bezpiecznie włączyć do niezależnego systemu VGK. Wymagałoby to zgodności środowiska Windows, a także osobnego sprawdzenia licencji i prawa do redystrybucji.

Możemy natomiast zaprojektować własny menedżer procesów VGK, czerpiąc inspirację z funkcji takich jak:
- kontrola priorytetów;
- przypisywanie procesów do rdzeni;
- ograniczanie nadmiernego zużycia zasobów;
- ochrona płynności działania systemu.

Nie kopiujemy kodu ani interfejsu programu. Najpierw powstaje bezpieczny podgląd i podstawowe operacje, a zaawansowane sterowanie dopiero po przygotowaniu mechanizmów planisty i kontroli uprawnień.

## 8. Kryteria uznania funkcji za gotową

- Testy pokazują, że blokada działa również bez otwartego panelu.
- Biała lista nie omija czarnej listy.
- Reguły dla plików wykrywają podmianę wskazanego programu.
- Błędna reguła może zostać cofnięta w trybie awaryjnym.
- Panel nie pokazuje fikcyjnych połączeń ani nie przypisuje ich procesom bez wiarygodnego powiązania.
- Menedżer procesów nie może przypadkowo zakończyć krytycznego procesu systemowego.
- Wszystkie istotne działania są rejestrowane lokalnie i widoczne dla użytkownika.

To jest dokumentacja projektu i kierunek wdrożenia, nie deklaracja, że zapora lub menedżer procesów już działają.
