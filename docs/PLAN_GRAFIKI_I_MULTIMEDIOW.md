# Plan grafiki, sterowników i multimediów VGK

## Cel

VGK ma zapewniać własną, niezależną warstwę obsługi grafiki oraz zestaw podstawowych aplikacji multimedialnych. Projekt musi działać również na starszych komputerach, w tym z 4 GB RAM, i nie może wymagać nowoczesnej karty graficznej do uruchomienia pulpitu.

Ten dokument opisuje plan. Nie oznacza, że sterowniki ani aplikacje są już zaimplementowane.

## 1. Zasady architektury grafiki

1. Warstwa graficzna VGK jest oddzielona od sterowników konkretnych kart.
2. Jądro nie może zależeć od jednego producenta ani jednego modelu GPU.
3. Sterowniki są dobierane na podstawie identyfikatorów urządzenia oraz rzeczywiście sprawdzonych możliwości.
4. Brak dedykowanego sterownika nie może uniemożliwić uruchomienia systemu.
5. Przyspieszenie sprzętowe jest opcjonalne; funkcje podstawowe muszą działać także programowo.
6. Sterowniki działają z ograniczonymi uprawnieniami, o ile architektura i etap rozwoju VGK na to pozwolą. Błąd sterownika nie powinien bezpośrednio uszkodzić jądra.
7. Aktualizacja sterownika musi mieć możliwość wycofania, a poprzedni sprawdzony sterownik lub tryb podstawowy musi pozostać dostępny.
8. Nie wolno deklarować zgodności z kartą tylko na podstawie nazwy producenta. Każdy model i zakres funkcji wymagają testów.

## 2. Poziomy obsługi obrazu

### Poziom A — obraz podstawowy

- Na komputerach uruchamianych przez UEFI wykorzystać dostępny tryb obrazu UEFI/GOP jako etap początkowy.
- Zapewnić podstawowe wyświetlanie obrazu i komunikatów diagnostycznych.
- Nie wymagać akceleracji 3D ani dużej ilości pamięci graficznej.
- Zachować tryb awaryjny o niskiej rozdzielczości, jeśli bardziej zaawansowany sterownik nie działa.

Tryb UEFI/GOP jest rozwiązaniem startowym, a nie docelowym sterownikiem GPU.

### Poziom B — wykrywanie urządzenia

Zebrać i zapisać co najmniej:
- producenta i identyfikator urządzenia PCI;
- identyfikator podsystemu i rewizję, jeśli są dostępne;
- klasę urządzenia;
- dostępną pamięć i możliwości urządzenia, jeśli można je bezpiecznie odczytać;
- obsługiwane tryby obrazu i funkcje;
- wersję sterownika oraz wynik jego inicjalizacji.

Nieznane urządzenie ma otrzymać status „nieznane — używany tryb podstawowy”, a nie być uznane za uszkodzone.

### Poziom C — sterowniki producentów

Docelowo rozwijać oddzielne rodziny sterowników:
- Intel;
- AMD;
- NVIDIA.

Nie zakładać, że wszystkie generacje danego producenta będą obsługiwane jednocześnie. Najpierw wybrać ograniczone, dobrze udokumentowane modele do testów, a później rozszerzać listę.

Sterowniki mogą korzystać z publicznej dokumentacji sprzętu i legalnie dostępnych interfejsów. Nie uzależniać podstawowego uruchomienia VGK od zamkniętych składników, których nie można bezpiecznie i legalnie rozpowszechniać.

### Poziom D — przyspieszenie

Po stabilnym uruchomieniu sterownika dodawać stopniowo:
- kopiowanie i wypełnianie obszarów obrazu z użyciem GPU;
- sprzętowe skalowanie i konwersję obrazu;
- akcelerację kompozycji okien;
- dekodowanie wideo;
- funkcje 3D tylko wtedy, gdy są potrzebne i przetestowane.

Każda funkcja ma mieć sprawdzenie możliwości urządzenia i bezpieczną ścieżkę programową.

## 3. Bezpieczne uruchamianie i odzyskiwanie

- Inicjalizacja sterownika odbywa się etapami i z kontrolą błędów.
- Przed zmianą aktywnego sterownika zachować możliwość powrotu do trybu podstawowego.
- Nieudana inicjalizacja nie może pozostawić użytkownika z trwałym czarnym ekranem.
- W razie awarii system powinien uruchomić tryb podstawowy i zapisać raport diagnostyczny.
- Aktualizacje sterowników powinny być weryfikowane, podpisywane i możliwe do cofnięcia.
- Testy najpierw prowadzić w środowisku emulowanym i na sprzęcie testowym; dopiero po ich przejściu planować testy na komputerach użytkowników.

## 4. Warstwa graficzna VGK

Należy zaprojektować wspólny interfejs pomiędzy pulpitem i aplikacjami a sterownikami. Aplikacje nie powinny komunikować się bezpośrednio z rejestrami GPU.

Warstwa powinna zapewnić:
- podstawowe powierzchnie obrazu i buforowanie;
- obsługę rozdzielczości i wielu ekranów w późniejszym etapie;
- obsługę kursora i odświeżania obrazu;
- raportowanie dostępnych możliwości;
- jednolity mechanizm zgłaszania błędów i przełączania na tryb podstawowy.

## 5. Aplikacje multimedialne VGK

### Aparat VGK
- wykrywanie kamer wbudowanych i USB;
- podgląd, wykonywanie zdjęć i nagrywanie filmów;
- wybór dostępnej rozdzielczości i jakości;
- samowyzwalacz oraz wybór miejsca zapisu;
- wyraźny wskaźnik aktywnej kamery i kontrola uprawnień.

### Edytor zdjęć VGK
- kadrowanie, obracanie i zmiana rozmiaru;
- jasność, kontrast, kolory i ostrość;
- tekst, strzałki, ramki i oznaczenia;
- filtry oraz cofanie i ponawianie zmian;
- obsługa popularnych formatów, w pierwszej kolejności PNG i JPEG, a następnie kolejne formaty zależnie od bibliotek i testów;
- oryginał pozostaje bez zmian, dopóki użytkownik nie zatwierdzi nadpisania.

### Odtwarzacz filmów VGK
- obsługa popularnych kontenerów i kodeków, rozwijana etapami;
- napisy i wybór ścieżki dźwiękowej;
- pełny ekran, regulacja głośności i prędkości;
- zapamiętywanie miejsca odtwarzania jako opcja;
- dekodowanie sprzętowe, jeśli sterownik i karta je obsługują; w przeciwnym razie dekodowanie programowe z kontrolą obciążenia.

### Galeria VGK
- przeglądanie zdjęć i filmów;
- sortowanie według dat i albumów;
- wyszukiwanie plików;
- pokazy slajdów;
- otwieranie zdjęć w edytorze i filmów w odtwarzaczu.

Aplikacje mają być oddzielne, aby awaria jednej nie blokowała pozostałych. Podstawowe funkcje powinny działać offline.

## 6. Wymagania dla słabszych komputerów

- ograniczone zużycie pamięci;
- brak obowiązkowego przyspieszenia 3D;
- możliwość wyłączenia animacji i efektów pulpitu;
- skalowanie jakości podglądu i odtwarzania do możliwości komputera;
- brak wymogu dedykowanej pamięci GPU o określonej wielkości;
- zachowanie responsywności systemu podczas odtwarzania filmu lub edycji zdjęć.

## 7. Kolejność realizacji

1. Ustabilizować rozruch UEFI i podstawowy obraz.
2. Zaprojektować interfejs warstwy graficznej i wykrywanie urządzeń PCI.
3. Dodać diagnostykę GPU oraz raport możliwości sprzętu.
4. Zbudować i przetestować podstawową obsługę wybranego, udokumentowanego urządzenia.
5. Dodać mechanizm powrotu do trybu podstawowego i cofania aktualizacji.
6. Rozszerzać obsługę rodzin Intel, AMD i NVIDIA według dostępnej dokumentacji i wyników testów.
7. Dodać akcelerację 2D, a później dekodowanie wideo i wybrane funkcje 3D.
8. Rozwijać aparat, edytor zdjęć, odtwarzacz i galerię, zaczynając od funkcji niewymagających akceleracji.

## 8. Testy i kryteria akceptacji

- start z trybem podstawowym bez dedykowanego sterownika;
- poprawne wykrywanie znanego i nieznanego urządzenia;
- brak zawieszenia systemu po błędzie inicjalizacji sterownika;
- możliwość powrotu do poprzedniego sterownika lub trybu podstawowego;
- testy rozdzielczości, odświeżania, wielu ekranów i uśpienia dopiero po wdrożeniu odpowiednich funkcji;
- testy zużycia pamięci na konfiguracji z 4 GB RAM;
- testy odtwarzania przy braku dekodowania sprzętowego;
- oddzielne oznaczanie wyników testów automatycznych, QEMU i fizycznego sprzętu.

## 9. Granica obecnego etapu

Na obecnym etapie jest to plan architektury. Nie oznacza działających sterowników Intel, AMD lub NVIDIA, pełnej akceleracji grafiki, aparatu ani aplikacji multimedialnych. Każda z tych funkcji wymaga osobnej implementacji i testów.
