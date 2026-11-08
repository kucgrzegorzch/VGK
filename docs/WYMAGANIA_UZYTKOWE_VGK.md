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

Ochrona ma działać lokalnie i nie może wymagać połączenia z usługą sztucznej inteligencji. Inteligentny Opiekun Systemu może pomagać w analizie i reagowaniu na problemy, ale nie może być jedyną barierą bezpieczeństwa.

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
- aplikację do tworzenia kopii zapasowych;
- przeglądarkę internetową;
- prosty edytor tekstu;
- archiwizator plików;
- podstawowe narzędzia diagnostyczne dostępne w zwykłym systemie, bez terminala naprawczego.

Aplikacja kopii zapasowych ma działać w zwykłym systemie. Użytkownik powinien móc wybrać dane do zapisania i miejsce kopii, w tym dysk lokalny albo pendrive, a po wykonaniu sprawdzić poprawność kopii. Tworzenie kopii nie może uruchamiać procedury przywracania systemu.

Operacje mogące usunąć dane (np. formatowanie, zmiana partycji) muszą mieć dodatkowe ostrzeżenie i potwierdzenie.

## 4. Pulpit i dolny pasek

Dolny pasek pulpitu ma obsługiwać automatyczne ukrywanie:
- po odsunięciu kursora od dolnej krawędzi pasek znika;
- po przesunięciu kursora do dolnej krawędzi ekranu pasek wysuwa się;
- użytkownik może wyłączyć automatyczne ukrywanie w ustawieniach;
- zachowanie musi być wygodne również na ekranach dotykowych.

## 5. Tryb awaryjny i odzyskiwanie systemu

Tworzenie zwykłych kopii zapasowych odbywa się w działającym VGK, za pomocą aplikacji opisanej w sekcji 3. **Przywracanie systemu z kopii jest dostępne wyłącznie w trybie awaryjnym** — nie na zwykłym pulpicie.

Ustalone funkcje trybu awaryjnego są ograniczone do trzech:
1. **Terminal naprawczy** — dostępny wyłącznie w trybie awaryjnym. Nie może być dostępny podczas zwykłej pracy systemu.
2. **Automatyczne migawki systemu** — dwie migawki w miesiącu; przy tworzeniu kolejnej najstarsza jest usuwana, aby zachować ustaloną rotację.
3. **Przywracanie systemu** — możliwość odtworzenia systemu z kopii znajdującej się na dysku komputera albo na pendrivie. Przed rozpoczęciem należy sprawdzić, czy kopia jest czytelna i zgodna; użytkownik musi potwierdzić operację, ponieważ może ona zastąpić aktualny stan systemu.

Tryb awaryjny ma być częścią odseparowanego środowiska uruchamiania i ratunkowego, z własnymi plikami. Awaria właściwego systemu nie powinna uniemożliwić uruchomienia środowiska ratunkowego. Należy zabezpieczyć jego pliki kontrolą integralności i oddzielić je od zwykłego systemu.

W zwykłym trybie pracy terminal naprawczy i funkcja przywracania systemu pozostają niedostępne. Za bieżące monitorowanie i wykrywanie problemów odpowiada **Inteligentny Opiekun Systemu**. Użytkownik korzysta z trybu awaryjnego przy poważnych awariach lub gdy zwykły system nie może się uruchomić.

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
5. Zbudować instalator, menedżer aplikacji i aplikację kopii zapasowych.
6. Rozwijać zgodność z programami Windows; później ocenić zgodność z programami Linux.
7. Dokończyć odseparowane środowisko ratunkowe i trzy ustalone funkcje trybu awaryjnego; bieżące monitorowanie powierzyć Inteligentnemu Opiekunowi Systemu.

**Status:** wymagania zapisane do realizacji etapami. Żadna z funkcji opisanych w tym dokumencie nie jest przez samo zapisanie uznana za gotową.


## 8. Tworzenie nośnika instalacyjnego i instalacja VGK

VGK ma otrzymać własną, prostą aplikację do przygotowywania pendrive'a instalacyjnego, podobną w obsłudze do Rufusa. Ma ona:
- wskazać pobrany obraz instalacyjny VGK i docelowy pendrive;
- przed zapisem sprawdzić podpis wydania i sumę kontrolną obrazu;
- sprawdzić, czy wybrano właściwy nośnik i wyraźnie ostrzec, że zapis usuwa jego dotychczasową zawartość;
- wymagać świadomego potwierdzenia przed rozpoczęciem zapisu;
- przygotowywać nośnik dla własnego mechanizmu uruchamiania VGK, początkowo UEFI, a dla starszego BIOS dopiero po ukończeniu i przetestowaniu osobnej ścieżki;
- po zapisie sprawdzić poprawność nagrania i zgłosić ewentualny błąd;
- nie wymagać od użytkownika terminala ani ręcznego formatowania pendrive'a.

Osobnym elementem jest **instalator VGK** uruchamiany z przygotowanego nośnika. Ma on prowadzić użytkownika krok po kroku przez wybór dysku i sposób instalacji, pokazywać skutki operacji oraz chronić istniejące dane. Nie wolno zakładać, że samo utworzenie pendrive'a instaluje system na komputerze.

## 9. Programy biurowe

VGK ma oferować zestaw programów biurowych, dostępny z pulpitu i możliwy do zainstalowania bez terminala:
- edytor tekstu do pism, dokumentów i raportów;
- arkusz kalkulacyjny do tabel, obliczeń i wykresów;
- program do tworzenia i wyświetlania prezentacji.

Priorytetem jest obsługa popularnych formatów dokumentów, w tym formatów OpenDocument oraz możliwie dobra wymiana plików z formatami Microsoft Office. Zgodność należy sprawdzać na rzeczywistych dokumentach; nie wolno obiecywać idealnego odwzorowania każdego zaawansowanego pliku.

Na początku należy ocenić możliwość dołączenia istniejącego, legalnie rozpowszechnianego pakietu biurowego albo jego bezpiecznej instalacji z poziomu menedżera aplikacji. Wybór konkretnego pakietu wymaga sprawdzenia licencji, wymagań sprzętowych, rozmiaru i zgodności z niezależnym VGK. Dołączenie aplikacji biurowych nie oznacza oparcia jądra ani podstaw systemu na Linuksie.
