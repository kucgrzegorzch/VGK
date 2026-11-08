# Wymagania użytkowe i plan funkcji VGK

Dokument zbiera wymagania zgłoszone przez właściciela projektu. Są to cele projektowe, a nie deklaracja, że funkcje już istnieją. Każdą funkcję należy oznaczyć jako zaplanowaną, w trakcie prac, gotową lub sprawdzoną na sprzęcie.

## 1. Zasada bezpieczeństwa systemu

VGK ma być projektowany tak, aby zwykły program nie mógł samodzielnie zmieniać chronionych plików systemowych ani uzyskiwać uprawnień administratora bez wyraźnej zgody.

Wymagane elementy:
- weryfikacja podpisu cyfrowego i pochodzenia instalatora przed uruchomieniem;
- kontrola integralności plików systemowych podczas uruchamiania i w czasie pracy;
- odmowa uruchomienia lub izolowanie pliku, którego podpis jest nieprawidłowy albo którego zawartość zmieniono;
- rozdzielenie uprawnień zwykłego użytkownika, aplikacji i administratora;
- ograniczanie dostępu aplikacji do plików, urządzeń, pamięci i usług, zgodnie z zasadą minimalnych uprawnień;
- bezpieczne aktualizacje z podpisem i kontrolą integralności; odzyskiwanie systemu odbywa się zgodnie z osobnym trybem awaryjnym opisanym niżej;
- dziennik zdarzeń bezpieczeństwa oraz czytelne ostrzeżenia dla użytkownika.

Ochrona ma działać lokalnie i nie może wymagać połączenia z usługą sztucznej inteligencji. Moduł inteligentnego doradcy może pomagać w analizie, ale nie może być jedyną barierą bezpieczeństwa.

Nie zakładamy, że sam podpis lub skanowanie zapewni pełną odporność na złośliwe oprogramowanie. Ochrona ma składać się z kilku niezależnych warstw.

## 2. Instalowanie programów

Preferowany sposób:
- użytkownik pobiera program z internetu albo strony jego producenta;
- VGK sprawdza podpis, wydawcę, integralność i wymagane uprawnienia;
- użytkownik uruchamia prosty instalator;
- instalator pokazuje, co zostanie dodane i do czego program będzie miał dostęp;
- program można łatwo odinstalować, a instalacja nie może po cichu podmieniać plików systemowych.

Nie wprowadzać Flatpak jako obowiązkowego ani domyślnego sposobu instalowania programów. Nie uzależniać zwykłego użytkownika od terminala.

### Zgodność z programami innych systemów — kolejność

1. **Programy Windows** — główny cel zgodności aplikacji zewnętrznych. Należy zbadać i rozwijać własne środowisko zgodności z programami Windows. Sam instalator nie wystarczy: potrzebna będzie warstwa zgodności z interfejsami systemowymi Windows. Nie można obiecać działania każdego programu, zwłaszcza tych zależnych od sterowników, zabezpieczeń jądra lub specjalistycznego sprzętu.
2. **Programy Linux** — możliwy późniejszy etap, po zapewnieniu zgodności ABI i potrzebnych usług. Nie oznacza to obowiązku używania Flatpak.
3. **Programy macOS** — na razie poza zakresem podstawowego planu. Wymagają odrębnej zgodności z formatem programów, bibliotekami i usługami Apple; nie należy obiecywać ich obsługi bez osobnego projektu i testów.

Priorytetem jest stabilny instalator i środowisko uruchamiania programów Windows, a nie pozorna zgodność ze wszystkimi systemami naraz.

## 3. Podstawowe aplikacje VGK

System powinien od pierwszej użytecznej wersji zawierać podstawowy zestaw narzędzi:
- menedżer plików;
- narzędzie do obsługi dysków i partycji: wykrywanie dysków, informacje o pojemności, sprawdzanie stanu, tworzenie i usuwanie partycji oraz formatowanie po wyraźnym potwierdzeniu;
- ustawienia systemowe;
- menedżer instalowania i usuwania aplikacji;
- narzędzie aktualizacji systemu;
- przeglądarka internetowa;
- prosty edytor tekstu;
- archiwizator plików;
- podstawowe narzędzia diagnostyczne dostępne w zwykłym systemie, bez terminala naprawczego.

Operacje mogące usunąć dane (np. formatowanie, zmiana partycji) muszą mieć dodatkowe ostrzeżenie i potwierdzenie.

## 4. Pulpit i dolny pasek

Dolny pasek pulpitu ma obsługiwać automatyczne ukrywanie:
- po odsunięciu kursora od dolnej krawędzi pasek znika;
- po przesunięciu kursora do dolnej krawędzi ekranu pasek wysuwa się;
- użytkownik może wyłączyć automatyczne ukrywanie w ustawieniach;
- zachowanie musi być wygodne również na ekranach dotykowych.

## 5. Tryb awaryjny i odzyskiwanie systemu

Odzyskiwanie VGK ma odbywać się przez odrębny tryb awaryjny, a nie przez zwykły pulpit.

Ustalone funkcje trybu awaryjnego są ograniczone do trzech:
1. **Terminal naprawczy** — dostępny wyłącznie w trybie awaryjnym. Nie może być dostępny podczas zwykłej pracy systemu.
2. **Automatyczne migawki systemu** — dwie migawki w miesiącu; przy tworzeniu kolejnej najstarsza jest usuwana, aby zachować ustaloną rotację.
3. **Odtworzenie systemu** — możliwość przywrócenia systemu z kopii znajdującej się na dysku komputera albo na pendrivie.

Tryb awaryjny ma być częścią odseparowanego środowiska uruchamiania i ratunkowego, z własnymi plikami. Awaria właściwego systemu nie powinna uniemożliwić uruchomienia środowiska ratunkowego. Należy zabezpieczyć jego pliki kontrolą integralności i oddzielić je od zwykłego systemu.

W zwykłym trybie pracy terminal naprawczy pozostaje niedostępny. Za bieżące monitorowanie i wykrywanie problemów odpowiada Strażnik VGK. Użytkownik korzysta z trybu awaryjnego przy poważnych awariach lub gdy zwykły system nie może się uruchomić.

Nie dodawać do zwykłego pulpitu osobnego, ogólnego narzędzia do ręcznego odtwarzania systemu — tę rolę pełni opisany tryb awaryjny.

## 6. Zgłoszenia brakujących funkcji

Jeśli użytkownik zapyta o funkcję powszechną w innych systemach, której VGK jeszcze nie ma, odpowiedź powinna być uczciwa i spokojna:
- jasno powiedzieć, że VGK jest na wczesnym etapie rozwoju i funkcja nie jest jeszcze dostępna;
- nie udawać, że funkcja istnieje;
- zapisać zgłoszenie w wykazie potrzeb;
- ocenić bezpieczeństwo, przydatność, nakład pracy i zależności;
- rozważyć wdrożenie w kolejnej aktualizacji lub wskazać realniejszy termin, jeśli wymaga to większych prac.

Nie należy obiecywać, że każda zgłoszona funkcja na pewno trafi do najbliższej aktualizacji. Priorytetem pozostają bezpieczeństwo, stabilność i sprawdzone działanie.

## 7. Kolejność realizacji

1. Dokończyć bezpieczne uruchamianie własnego jądra VGK.
2. Zbudować podstawy pamięci, wyjątków, przerwań i sterowników.
3. Wdrożyć ochronę plików systemowych, uprawnienia i bezpieczny mechanizm aktualizacji.
4. Przygotować podstawowe narzędzia systemowe i pulpit, w tym ukrywany dolny pasek.
5. Zbudować instalator oraz menedżer aplikacji.
6. Rozwijać zgodność z programami Windows; później ocenić zgodność z programami Linux.
7. Dokończyć odseparowane środowisko ratunkowe i trzy ustalone funkcje trybu awaryjnego; bieżące monitorowanie powierzyć Strażnikowi VGK.

**Status:** wymagania zapisane do realizacji etapami. Żadna z funkcji opisanych w tym dokumencie nie jest przez samo zapisanie uznana za gotową.
