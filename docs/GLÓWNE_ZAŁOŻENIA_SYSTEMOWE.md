# Główne założenia systemowe VGK

**Status:** propozycja architektoniczna; dokument opisuje zasady projektowe, a nie funkcje już wdrożone w systemie.

## 1. Cel dokumentu

Ten dokument stanowi nadrzędny, czytelny dla użytkownika zbiór zasad, według których projektuje się, rozwija i ocenia VGK. Nie jest to zwykła lista ustawień systemowych.

W przyszłym interfejsie VGK lista ma być dostępna użytkownikowi w trybie **tylko do odczytu**. Nie będzie można zmieniać jej bezpośrednio z poziomu zwykłych ustawień ani przez aplikacje użytkownika.

Zmiany zasad wymagają kontrolowanej zmiany projektu: uzasadnienia, oceny wpływu na bezpieczeństwo i zgodność, testów oraz zatwierdzenia przez właściciela projektu przed wydaniem. Zasada tylko do odczytu nie może służyć do odebrania właścicielowi projektu kontroli nad jego systemem.

## 2. Nadrzędne zasady

1. **Bezpieczeństwo jest częścią architektury.** Ochrona systemu nie może zależeć od instalacji zewnętrznego antywirusa ani od usług płatnych lub połączenia z Internetem.
2. **Integralność jądra.** Zwykłe aplikacje, w tym zewnętrzne programy ochronne, nie mogą samowolnie modyfikować jądra, pamięci jądra, tablic stron ani uprzywilejowanego stanu procesora.
3. **Kontrolowane uprawnienia.** Każdy komponent otrzymuje wyłącznie uprawnienia niezbędne do swojej funkcji. Uprzywilejowane operacje są wykonywane przez kontrolowane mechanizmy VGK.
4. **Ochrona przed nieautoryzowanym kodem.** Docelowa architektura ma weryfikować pochodzenie i integralność komponentów systemowych oraz stosować jasno określoną politykę uruchamiania aplikacji.
5. **Odporność i odzyskiwanie.** Projekt powinien uwzględniać wykrywanie usterek, bezpieczne tryby awaryjne, kopie/rollback tam, gdzie to możliwe, oraz ochronę danych użytkownika podczas napraw.
6. **Przejrzystość.** Użytkownik powinien otrzymywać zrozumiałe informacje o istotnych decyzjach bezpieczeństwa, blokadach i działaniach naprawczych.
7. **Prywatność i wydajność.** Ochrona ma ograniczać zbieranie danych, działać lokalnie w podstawowym zakresie i kontrolować zużycie zasobów, także na komputerach z 4–8 GB RAM.
8. **Kompatybilność bez osłabiania ochrony.** Obsługa sprzętu i aplikacji nie może wymagać wyłączania podstawowych zabezpieczeń ani przyznawania nieograniczonego dostępu.
9. **Testowalność.** Każda istotna funkcja bezpieczeństwa musi mieć określone warunki poprawności, testy pozytywne i negatywne oraz jawnie opisane ograniczenia.
10. **Bezpieczne aktualizacje.** Aktualizacje systemu powinny być weryfikowane, możliwe do kontrolowanego wdrożenia i posiadać przewidzianą ścieżkę odzyskiwania po niepowodzeniu.

## 3. Odpowiedzialność za ocenę techniczną

Asystent wspierający rozwój VGK otrzymał od właściciela projektu upoważnienie do samodzielnego prowadzenia analiz technicznych, przygotowywania propozycji, sprawdzania zmian i testów oraz wykonywania uzgodnionych czynności w repozytorium.

W ramach tego upoważnienia powinien:
- stawiać bezpieczeństwo, poprawność i stabilność ponad szybkość dodawania funkcji;
- sprawdzać zależności, skutki uboczne i możliwość odzyskania systemu przed proponowaniem zmian;
- nie przedstawiać funkcji planowanych lub częściowo przetestowanych jako gotowych;
- pozostawiać niezweryfikowane lub ryzykowne zmiany niescalone i jasno opisywać powód;
- dokumentować wyniki testów oraz rozróżniać testy statyczne, kompilację, testy emulatora i testy na fizycznym sprzęcie.

To upoważnienie nie oznacza nieomylności ani gwarancji pełnego bezpieczeństwa. Nie zastępuje testów, niezależnej kontroli ani ostatecznej odpowiedzialności właściciela projektu za zatwierdzanie wydań i kierunku rozwoju.

## 4. Zasady zmieniania tego dokumentu

- Lista jest przeznaczona do odczytu w interfejsie użytkownika.
- Użytkownik może zgłosić propozycję zmiany, ale nie edytuje zasad bezpośrednio w ustawieniach.
- Każda propozycja zmiany wymaga analizy wpływu, uzasadnienia i odpowiednich testów.
- Zmiany powinny być wprowadzane przez kontrolowany przegląd kodu/dokumentacji i zatwierdzane przez właściciela projektu.
- Nie wolno wprowadzać zmian w sposób ukryty ani obchodzić procesu zatwierdzania.

## 5. Granica między założeniem a wdrożeniem

Umieszczenie zasady na tej liście nie oznacza, że odpowiadający jej mechanizm już istnieje. Stan implementacji musi być opisany osobno i potwierdzony kodem oraz testami. W szczególności VGK pozostaje rozwijanym projektem, a nie ukończonym i w pełni zabezpieczonym systemem operacyjnym.
