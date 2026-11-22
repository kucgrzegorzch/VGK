# Interfejs użytkownika Inteligentnego Opiekuna Systemu (IOS)

**Status:** propozycja UX i kontraktu interfejsu. To projekt przyszłego interfejsu VGK, nie zaimplementowany ekran ani działająca usługa IOS. Dokument nie przyznaje interfejsowi ani IOS dodatkowych uprawnień.

## 1. Cel interfejsu

Interfejs IOS ma dawać użytkownikowi spokojny, zrozumiały obraz kondycji systemu i tego, co Opiekun robi lub zaleca. Nie powinien przypominać konsoli administratora, panelu diagnostycznego pełnego liczb ani typowego pulpitu antywirusa.

Najważniejsze pytania, na które ekran ma odpowiadać bez szukania po menu:
- Czy VGK działa prawidłowo?
- Czy IOS wykrył coś, co wymaga mojej uwagi?
- Czy system sam wykonał dozwolone działanie, czy tylko je proponuje?
- Czy potrzebna jest moja decyzja?
- Co dokładnie się zmieni i czy można to cofnąć?

Interfejs ma tłumaczyć stan, nie straszyć. Nie może ukrywać poważnego problemu pod ogólnym komunikatem „Wszystko w porządku”.

## 2. Zasady projektowe

1. **Najpierw znaczenie, potem szczegóły:** użytkownik widzi prosty opis i wagę zdarzenia; dane techniczne są dostępne po rozwinięciu.
2. **Jeden główny punkt orientacyjny:** ekran główny rozpoczyna się od czytelnego stanu VGK, a nie od tabeli procesów, wykresów lub listy modułów.
3. **Wyraźne rozróżnienie statusów:** „prawidłowo”, „wymaga uwagi”, „działanie w toku”, „oczekuje na decyzję”, „nie można potwierdzić”.
4. **Bez pozornej pewności:** brak danych lub niedostępny moduł nie może być przedstawiany jako stan prawidłowy.
5. **Bez alarmizmu:** kolor nie jest jedynym nośnikiem znaczenia; status ma również ikonę, nazwę i opis.
6. **Kontrola użytkownika:** działania wymagające zgody pokazują zakres, powód, możliwe skutki i możliwość odmowy.
7. **Bez automatycznych niespodzianek:** nie umieszczamy nieodwracalnych działań pod jednym przyciskiem „Napraw”.
8. **Spójność z VGK:** własna typografia, ikony i rytm układu; nie kopiujemy wyglądu paneli Linux ani Windows.
9. **Lekkość:** ekran ma działać płynnie na komputerach z 4–8 GB RAM i nie wymagać stale animowanych wykresów.
10. **Dostępność:** obsługa klawiaturą, czytelny kontrast, skalowanie interfejsu i etykiety dla czytników ekranu.

## 3. Główna nawigacja

Proponowane sekcje:

| Sekcja | Co zawiera |
|---|---|
| **Przegląd** | ogólny stan systemu, najważniejsze ostrzeżenie i ostatnie działania |
| **Ochrona** | stan integralności, plików, aplikacji, sterowników i sieci |
| **Diagnostyka** | wyniki kontroli, wykryte usterki i zalecane badania |
| **Historia** | chronologiczny zapis obserwacji, decyzji, zgód i rezultatów |
| **Odzyskiwanie** | dostępne punkty przywracania i procedury naprawcze, jeśli są rzeczywiście dostępne |
| **Ustawienia Opiekuna** | zakres powiadomień, harmonogram kontroli, prywatność i preferencje |

Sekcje niedostępne w danej wersji VGK powinny być oznaczone jako „jeszcze niedostępne” albo w ogóle ukryte zgodnie z decyzją produktu. Nie wolno prezentować planowanego modułu tak, jakby już działał.

## 4. Ekran główny – Przegląd

Ekran powinien mieć następującą hierarchię:

### A. Nagłówek Opiekuna

- nazwa: **Inteligentny Opiekun Systemu**;
- krótki opis: „Czuwam nad stanem i bezpieczeństwem VGK”;
- dyskretny znacznik, czy usługa jest aktywna, zatrzymana, czy jej stan jest nieznany;
- dostęp do pomocy i ustawień.

Nie używać twierdzenia „Opiekun chroni system”, jeżeli w danej wersji działają jedynie wybrane moduły.

### B. Karta stanu VGK

Jeden dominujący, prosty obszar przedstawiający stan całego systemu. Proponowane stany:

- **System działa prawidłowo** – wszystkie wymagane kontrole zakończyły się poprawnie.
- **Wymaga uwagi** – wykryto problem, ale nie ma potwierdzonego bezpośredniego zagrożenia.
- **Ochrona ograniczona** – co najmniej jeden istotny mechanizm nie działa lub nie może potwierdzić swojego stanu.
- **Wymagana decyzja** – bez zgody użytkownika nie można wykonać konkretnego działania.
- **Trwa sprawdzanie** – kontrola jest w toku; pokazujemy jej nazwę i postęp, jeśli można go wiarygodnie zmierzyć.
- **Stan nieznany** – brakuje wiarygodnych danych. To nie jest równoważne stanowi prawidłowemu.

Stan ogólny wynika z najpoważniejszego aktualnego stanu składowego, z uwzględnieniem jego ważności i aktualności. Nie wolno uśredniać krytycznej awarii z wieloma poprawnymi kontrolami.

### C. Najważniejsza sprawa

Jeśli istnieje problem wymagający uwagi, pokazujemy pojedynczy priorytetowy wpis:
- co się stało;
- czego dotyczy;
- co IOS już zrobił;
- czego jeszcze nie zrobił;
- czy potrzebna jest decyzja użytkownika;
- przycisk prowadzący do szczegółów.

Jeśli nie ma pilnej sprawy, obszar pokazuje ostatnią zakończoną kontrolę i jej wynik.

### D. Ostatnie działania

Krótka lista maksymalnie kilku najnowszych zdarzeń, np.:
- „Kontrola integralności zakończona”;
- „Zablokowano uruchomienie niezweryfikowanego składnika”;
- „Kopia zapasowa nie została zmieniona”;
- „Oczekuje na zgodę: przywrócenie pliku systemowego”.

Każdy wpis rozróżnia obserwację, decyzję i wykonany rezultat. Szczegóły prowadzą do Historii.

## 5. Widok szczegółów zdarzenia

Każde istotne zdarzenie ma czytelną kartę szczegółów z sekcjami:

1. **Co wykryto** – opis bez niepotrzebnego żargonu.
2. **Dlaczego Opiekun zwrócił uwagę** – źródło i reguła, która zadziałała.
3. **Decyzja** – zezwolono, odmówiono, ograniczono lub wymagane jest zatwierdzenie.
4. **Wykonane działanie** – oddzielone od samej decyzji.
5. **Wynik kontroli** – potwierdzony, niepowodzenie, częściowy albo nieznany.
6. **Co dalej** – bezpieczne następne kroki.
7. **Szczegóły techniczne** – identyfikatory zdarzeń, kody przyczyn, wersja polityki i dane diagnostyczne, jeśli użytkownik chce je zobaczyć.

Jeśli nie ma potwierdzenia wykonania, interfejs pokazuje „wynik niepotwierdzony”, a nie „naprawiono”.

## 6. Ekran zgody administratora

Prośba o zgodę musi być osobnym, wyraźnym widokiem, a nie małym komunikatem z przyciskiem „OK”.

Powinna pokazywać:
- nazwę operacji i konkretny zasób;
- powód, dla którego IOS ją proponuje;
- co zostanie zmienione;
- możliwe ryzyko i wpływ na pliki użytkownika;
- czy operacja jest odwracalna;
- czy zgoda jest jednorazowa i kiedy wygaśnie;
- co stanie się po odmowie.

Dostępne decyzje:
- **Zezwól na tę operację** – wyłącznie dla dokładnie opisanego zakresu;
- **Odmów**;
- **Pokaż szczegóły**.

Nie stosować ogólnego przycisku „Zezwól Opiekunowi”. Brak odpowiedzi nie jest zgodą. Interfejs nie może samodzielnie rozszerzać zakresu operacji po zatwierdzeniu.

## 7. Historia – widok Dziennika Decyzji

Historia powinna być osią czasu z filtrowaniem:
- wszystkie zdarzenia;
- ostrzeżenia;
- decyzje Opiekuna;
- działania wykonane;
- prośby o zgodę;
- błędy i przerwane operacje.

Wpisy powiązane jednym przypadkiem powinny być wizualnie zgrupowane. Użytkownik może rozwinąć przypadek, aby zobaczyć kolejno obserwację, walidację, decyzję, zgodę, wykonanie i weryfikację.

Interfejs ma wyraźnie oznaczać:
- lukę w numeracji;
- brak wyniku po rozpoczęciu operacji;
- niepewny czas;
- uszkodzenie lub brak możliwości weryfikacji integralności;
- wpisy pochodzące z niepełnego lub starszego formatu.

Eksport historii jest dostępny jako osobna czynność i ma ostrzegać o danych, które mogą ujawniać informacje o systemie. Sam interfejs nie może modyfikować źródłowego dziennika.

## 8. Powiadomienia

Powiadomienia dzielimy na trzy poziomy:

- **Informacyjne** – kontrola zakończona, aktualizacja gotowa, raport dostępny. Nie wymagają natychmiastowej reakcji.
- **Ważne** – wykryto problem wymagający sprawdzenia. Powiadomienie pozostaje w centrum powiadomień.
- **Krytyczne** – system zablokował niebezpieczną operację lub ochrona jest istotnie ograniczona. Powiadomienie jest widoczne i opisuje bezpieczny następny krok.

Nie należy wysyłać wielu powiadomień o tym samym zdarzeniu. Powiązane komunikaty powinny być grupowane, a powiadomienia informacyjne nie mogą zasłaniać pracy użytkownika.

## 9. Język i nazewnictwo

Interfejs używa krótkich, jednoznacznych komunikatów. Preferowane określenia:
- „Sprawdzono” zamiast „Skan zakończony pomyślnie”, gdy wykonano tylko kontrolę;
- „Zablokowano uruchomienie” zamiast niejasnego „Zagrożenie usunięte”;
- „Wymaga Twojej decyzji” zamiast „Błąd autoryzacji”;
- „Nie udało się potwierdzić” zamiast sugerowania sukcesu;
- „Przywróć” tylko wtedy, gdy istnieje rzeczywista, sprawdzona możliwość przywrócenia.

Komunikaty techniczne mogą być dostępne po rozwinięciu. Nie należy straszyć użytkownika terminami takimi jak „atak”, „wirus” czy „uszkodzenie” bez wiarygodnych przesłanek.

## 10. Stany brzegowe i awarie

Interfejs musi mieć określone zachowanie dla sytuacji:
- IOS nie uruchomił się;
- część modułów nie odpowiada;
- dziennik jest niedostępny lub uszkodzony;
- zegar systemowy jest niepewny;
- kontrola trwa dłużej niż przewidywano;
- system przechodzi w tryb ograniczony lub odzyskiwania;
- operacja została przerwana przez restart;
- brak miejsca uniemożliwia zapis nowego zdarzenia.

W każdym przypadku należy pokazać, co jest znane, czego nie udało się ustalić i jaki jest bezpieczny następny krok. Nie wolno wyświetlać zielonego statusu ogólnego tylko dlatego, że nie otrzymano błędu.

## 11. Wymagania dostępności i wydajności

- pełna obsługa klawiaturą i widoczny fokus;
- etykiety tekstowe niezależne od koloru;
- skalowanie tekstu i elementów;
- ograniczenie animacji, z możliwością ich wyłączenia;
- brak stale odświeżanych ciężkich wykresów na ekranie głównym;
- aktualizacja widoku zdarzeniami, bez ciągłego intensywnego odpytywania;
- długie operacje nie mogą blokować interfejsu;
- widok ma zachować czytelność przy małej rozdzielczości i dużym skalowaniu.

## 12. Kontrakt danych i bezpieczeństwo interfejsu

Interfejs jest klientem IOS, a nie źródłem decyzji. Powinien otrzymywać wersjonowane, ograniczone komunikaty o stanie i zdarzeniach.

- Interfejs nie podejmuje decyzji polityki i nie wykonuje operacji uprzywilejowanych.
- Każda prośba o działanie jest przekazywana do IOS jako konkretne żądanie; IOS ponownie sprawdza politykę i uprawnienia.
- Widok nie może ufać tekstowi pochodzącemu z nazw plików, aplikacji ani innych niezaufanych źródeł. Treść jest kodowana i prezentowana jako dane.
- Zgoda jest uwierzytelniana poza samym przyciskiem interfejsu i wiązana z zakresem operacji.
- Utrata połączenia z IOS powoduje wyświetlenie stanu „brak połączenia / stan nieznany”, a nie zachowanie ostatniego statusu jako aktualnego.
- Historia jest tylko do odczytu; eksport korzysta z kontrolowanej funkcji IOS.
- Interfejs nie może wywołać bezpośrednio funkcji jądra ani zmieniać polityki.

## 13. Etapy realizacji

1. Zatwierdzenie hierarchii informacji i nazw statusów.
2. Przygotowanie makiet ekranów Przegląd, szczegóły zdarzenia, zgoda i Historia.
3. Ustalenie wersjonowanego modelu danych prezentowanych przez interfejs.
4. Prototyp statyczny na danych demonstracyjnych, bez połączenia z funkcjami uprzywilejowanymi.
5. Testy czytelności, dostępności i zachowania przy braku danych.
6. Implementacja odczytowego interfejsu po powstaniu podstawowego źródła stanu IOS.
7. Dodanie kontrolowanych żądań i zgód dopiero po wdrożeniu kontraktu uprawnień oraz dziennika.

Każdy etap powinien być oceniany oddzielnie. Makieta ani prototyp nie są dowodem działania IOS.

## 14. Kryteria akceptacji projektu

- Użytkownik potrafi rozpoznać ogólny stan systemu bez otwierania szczegółów.
- Status nieznany lub niepełny nigdy nie jest prezentowany jako prawidłowy.
- Decyzja, wykonanie i weryfikacja są rozróżnione w każdym widoku.
- Prośba o zgodę jasno określa pojedynczą operację i jej skutki.
- Nie ma przycisku pozwalającego zatwierdzić nieograniczone uprawnienia.
- Historia pozwala prześledzić cały przypadek i wykryte nieciągłości.
- Awaria IOS lub dziennika nie powoduje fałszywego statusu „bezpiecznie”.
- Ekran główny pozostaje lekki i czytelny na docelowym sprzęcie VGK.

## 15. Powiązane dokumenty

- Architektura IOS: `docs/architecture/intelligent-system-caretaker.md`.
- Kontrakt uprawnień IOS: `docs/architecture/ios-authority-contract.md`.
- Dziennik Decyzji IOS: `docs/architecture/ios-decision-audit-log.md`.
- Model zagrożeń: `docs/architecture/threat-model.md`.

Projekt interfejsu podlega tym samym granicom zaufania co IOS. Przy konflikcie wygoda interfejsu nie może osłabić polityki ani ochrony jądra.
