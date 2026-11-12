# Plan grafiki, sterowników i multimediów VGK

Status: dokument projektowy. Opisuje docelową architekturę i kolejność prac; nie oznacza, że sterowniki, pulpit ani aplikacje multimedialne są już zaimplementowane.

## 1. Cele i zasady

VGK ma zapewniać:
- czytelny obraz i podstawowy interfejs także wtedy, gdy nie ma dedykowanego sterownika GPU;
- obsługę kart Intel, AMD i NVIDIA w miarę możliwości udokumentowanych interfejsów i dostępnych specyfikacji;
- rozdzielczość ekranu, odświeżanie, skalowanie i docelowo wiele monitorów;
- akcelerację sprzętową, gdy sterownik i karta bezpiecznie ją udostępniają;
- lekkie aplikacje do zdjęć i filmów, działające także na komputerach z 4 GB RAM;
- zachowanie trybu awaryjnego i możliwość powrotu do prostego obrazu po błędzie sterownika.

Priorytetem jest stabilność, zgodność i bezpieczeństwo, a nie maksymalna wydajność na każdej karcie od pierwszego wydania. Warstwa graficzna jest oddzielona od sterowników, a aplikacje nie komunikują się bezpośrednio z rejestrami GPU. Brak dedykowanego sterownika nie może uniemożliwić uruchomienia systemu.

## 1.1. Stan wykonania – pierwszy etap framebufferu

W kodzie ładowacza UEFI dodano pobieranie bieżącego trybu Graphics Output Protocol (GOP) z uchwytu konsoli. Obecna implementacja:
- rozpoznaje standardowe układy pikseli RGB i BGR, po 8 bitów na kanał;
- sprawdza wskaźnik i rozmiar informacji o trybie;
- sprawdza adres, rozmiar bufora, wymiary obrazu i liczbę pikseli w wierszu;
- odrzuca tryby bit-mask i BLT-only, których formatów VGK jeszcze nie obsługuje;
- przekazuje opis framebufferu w istniejącym protokole startowym, jeśli wszystkie kontrole przejdą;
- pozostawia flagę framebuffer wyłączoną, gdy GOP jest niedostępny lub format nieobsługiwany.

Dodano niezależne testy walidacji formatu, kroku wiersza, wymaganego rozmiaru i przepełnień adresów. To nadal tylko przekazanie opisu bufora. Jądro nie rysuje jeszcze pulpitu ani nie zmienia trybu ekranu. Wyniki testów CI dla tej zmiany są sprawdzane osobno.

## 2. Architektura grafiki

### 2.1. Warstwa obrazu podstawowego

Pierwszy etap wykorzystuje framebuffer przekazany przez UEFI Graphics Output Protocol (GOP), o ile firmware go udostępnia. VGK sprawdzi adres bufora, rozmiar, format pikseli, szerokość linii i zakres pamięci przed użyciem.

Ta warstwa ma zapewnić:
- wyświetlenie komunikatów startowych;
- prosty ekran diagnostyczny i ratunkowy;
- podstawowe rysowanie pikseli, prostokątów, tekstu i kursora;
- bezpieczny powrót do obrazu podstawowego, jeśli sterownik GPU nie uruchomi się poprawnie.

GOP jest interfejsem firmware do obrazu, a nie pełnym sterownikiem karty. Nie zapewnia sam z siebie akceleracji 3D ani wszystkich funkcji monitora. Na platformach bez dostępnego GOP trzeba będzie osobno zaprojektować obsługiwany tryb zgodności; nie należy zakładać, że historyczny VGA wystarczy dla nowoczesnych kart.

### 2.2. Wykrywanie urządzeń

Po uruchomieniu podstawowego obrazu VGK będzie rozpoznawać urządzenia PCI/PCI Express:
- producenta i identyfikator urządzenia;
- klasę i podklasę urządzenia;
- identyfikatory podsystemu i rewizję, jeśli są dostępne;
- zasoby i zakresy pamięci przypisane urządzeniu;
- wymagane przerwania i możliwości urządzenia, dopiero gdy ich obsługa będzie gotowa.

Nieznany model nie może być automatycznie uznawany za uszkodzony. System ma użyć bezpiecznej warstwy podstawowej i pokazać użytkownikowi, że karta nie ma jeszcze dedykowanego sterownika VGK.

### 2.3. Warstwa sterowników

Sterowniki będą oddzielone od aplikacji i od wspólnego interfejsu grafiki. Docelowe elementy:
- wspólny interfejs urządzenia graficznego;
- zarządzanie pamięcią obrazu i buforami;
- kontrola trybów ekranu i wyjść obrazu;
- synchronizacja pracy GPU i obsługa błędów;
- opcjonalne funkcje 2D i 3D;
- kontrolowane ładowanie sterowników z weryfikacją integralności i pochodzenia.

Sterownik, który zawiesza się lub zgłasza błędny stan, nie może bez potrzeby zatrzymać całego systemu. Tam, gdzie architektura i uprawnienia na to pozwolą, obsługa urządzenia powinna być izolowana od aplikacji; krytyczne operacje jądra pozostaną minimalne. Nie wolno ładować niezweryfikowanego kodu tylko po to, aby obsłużyć nową kartę.

### 2.4. Kolejność obsługi producentów

Nie należy obiecywać jednoczesnej, pełnej obsługi wszystkich generacji Intel, AMD i NVIDIA. Każda rodzina GPU ma własne wymagania, firmware, formaty poleceń i ograniczenia.

Kolejność:
1. uniwersalny obraz przez GOP;
2. wykrywanie kart i raportowanie możliwości;
3. podstawowa obsługa wybranych, dobrze udokumentowanych urządzeń;
4. sterowniki 2D i akceleracja kompozycji pulpitu;
5. akceleracja 3D i obliczeniowa tylko po przygotowaniu pamięci, synchronizacji, izolacji i testów;
6. rozszerzanie listy urządzeń na podstawie rzeczywistych testów.

Sterowniki mogą korzystać z publicznej dokumentacji sprzętu i legalnie dostępnych interfejsów. Nie uzależniamy podstawowego uruchomienia VGK od zamkniętych składników, których nie można bezpiecznie i legalnie rozpowszechniać.

### 2.5. Pamięć i bezpieczeństwo GPU

Przed uruchomieniem zaawansowanego sterownika VGK musi mieć bezpieczne mapowanie pamięci urządzeń, kontrolę zakresów DMA, zarządzanie buforami, synchronizację i obsługę przerwań. IOMMU należy wykorzystać, jeśli platforma je udostępnia i VGK ma gotową obsługę; nie wolno zakładać, że jest zawsze dostępne. Funkcje GPU nie mogą omijać zasad ochrony pamięci systemu.

## 3. Pulpit i interfejs użytkownika

Po ustabilizowaniu obrazu podstawowego powstanie warstwa okien i pulpitu:
- okna, pasek zadań, menu i ustawienia ekranu;
- skalowanie interfejsu i czytelne czcionki;
- obsługa myszy i klawiatury;
- podstawowa kompozycja 2D;
- ustawienia rozdzielczości i częstotliwości odświeżania;
- docelowo wiele monitorów i różne skale obrazu.

Pulpit ma działać również bez akceleracji GPU, korzystając z renderowania programowego. Akceleracja będzie dodatkiem, a nie warunkiem uruchomienia systemu. Użytkownik będzie mógł wyłączyć animacje i efekty.

## 4. Zestaw multimedialny VGK

Aplikacje będą osobnymi programami korzystającymi ze wspólnych bibliotek obrazu, dźwięku i plików. Awaria jednej aplikacji nie może uszkodzić pozostałych. Podstawowe funkcje mają działać offline.

### 4.1. Aparat VGK

Planowane funkcje:
- wykrywanie kamer wbudowanych i USB;
- podgląd obrazu;
- wykonywanie zdjęć;
- nagrywanie filmu, gdy obsługa kamery i dźwięku będzie gotowa;
- wybór rozdzielczości i jakości;
- samowyzwalacz i wybór miejsca zapisu;
- widoczny wskaźnik użycia kamery i mikrofonu oraz kontrola uprawnień.

Na początek preferowana będzie zgodność z powszechnym standardem USB Video Class (UVC), jeśli urządzenie go obsługuje. Nie każda kamera będzie działać od razu.

### 4.2. Edytor zdjęć VGK

Planowane funkcje:
- kadrowanie, obracanie i zmiana rozmiaru;
- jasność, kontrast, nasycenie i balans kolorów;
- ostrość i podstawowe filtry;
- tekst, strzałki, ramki i oznaczenia;
- cofanie i ponawianie zmian;
- zapis kopii lub wersji edytowanej.

Oryginał nie będzie nadpisywany bez wyraźnej zgody użytkownika. Początkowe formaty docelowe: PNG i JPEG; kolejne formaty, np. WebP, będą dodawane po wdrożeniu i przetestowaniu bibliotek.

### 4.3. Odtwarzacz filmów VGK

Planowane funkcje:
- odtwarzanie popularnych kontenerów i kodeków, zależnie od dostępnych bibliotek;
- pauza, przewijanie, pełny ekran i regulacja głośności;
- napisy i wybór ścieżki dźwiękowej;
- regulacja prędkości;
- zapamiętywanie miejsca oglądania, jeśli użytkownik tego chce;
- dekodowanie sprzętowe, gdy GPU i sterownik je obsługują, oraz tryb programowy jako wariant zgodności.

Kontener pliku (np. MP4 lub MKV) i kodek obrazu/dźwięku to różne rzeczy. Samo rozpoznanie rozszerzenia pliku nie oznacza obsługi wszystkich kodeków znajdujących się w środku. Obsługiwane formaty będą zależeć również od bibliotek, licencji i testów bezpieczeństwa.

### 4.4. Galeria VGK

Planowane funkcje:
- przeglądanie zdjęć i filmów;
- sortowanie według nazwy i daty;
- albumy i wyszukiwanie;
- pokazy slajdów;
- otwieranie zdjęć w edytorze i filmów w odtwarzaczu.

Indeksowanie biblioteki będzie opcjonalne i ograniczone, aby nie obciążać komputerów z małą ilością pamięci.

## 5. Dźwięk i urządzenia wejściowe

Odtwarzacz filmów i nagrywanie wymagają osobnej warstwy dźwięku. Trzeba zaprojektować wykrywanie urządzeń audio, wybór wejścia i wyjścia, regulację głośności, podstawowe miksery oraz obsługę mikrofonu z uprawnieniami. Nie należy uzależniać pierwszego pulpitu od gotowego dźwięku; dźwięk może być wdrażany jako osobny etap.

## 6. Wydajność i komputery o skromnych zasobach

VGK ma działać na komputerach z 4–8 GB RAM. Dlatego:
- pulpit musi mieć tryb bez akceleracji;
- dekodowanie i przetwarzanie obrazu powinno używać małych buforów i ograniczonej liczby wątków;
- aplikacje nie mogą stale trzymać w pamięci całej biblioteki zdjęć i filmów;
- sprzętowe dekodowanie wideo jest opcjonalne;
- użytkownik będzie mógł wyłączyć efekty i animacje;
- pamięć GPU i pamięć systemowa będą monitorowane osobno, gdy sprzęt udostępni takie informacje.

## 7. Bezpieczeństwo i prywatność

- Kamera i mikrofon nie są używane bez działania aplikacji i zgody użytkownika.
- Widoczny wskaźnik informuje o aktywnym użyciu kamery i mikrofonu.
- Aplikacje otrzymują tylko potrzebne uprawnienia.
- Sterowniki i aktualizacje są weryfikowane pod kątem integralności i pochodzenia.
- Aktualizacja sterownika musi mieć możliwość wycofania do poprzedniej sprawdzonej wersji.
- Funkcje sieciowe i kodeki będą dodawane z uwzględnieniem aktualizacji bezpieczeństwa i licencji.

## 8. Testowanie

Każdy etap będzie miał osobne testy:
- emulowany framebuffer: formaty pikseli, rozmiar, wyrównanie i granice bufora;
- wykrywanie PCI: brak urządzenia, nieznany identyfikator i błędne zasoby;
- sterownik: poprawna inicjalizacja, błędne odpowiedzi urządzenia, timeout i kontrolowany powrót;
- grafika: rozdzielczość, odświeżanie, zmiana trybu i brak akceleracji;
- pamięć: ograniczona pamięć RAM i pamięć obrazu;
- multimedia: pliki uszkodzone, nieobsługiwane kodeki, brak kamery i odłączanie urządzenia;
- bezpieczeństwo: brak uprawnień do kamery/mikrofonu, nieprawidłowy sterownik i wycofanie aktualizacji;
- testy QEMU/OVMF, a później osobne testy na rzeczywistych kartach Intel, AMD i NVIDIA.

Wynik testu emulatora nie będzie przedstawiany jako potwierdzenie działania na fizycznej karcie.

## 9. Proponowana kolejność wdrażania

1. Stabilny framebuffer GOP i tekst/rysowanie awaryjne.
2. Wykrywanie PCI i bezpieczny raport urządzeń graficznych.
3. Testy trybów obrazu i powrotu do trybu podstawowego.
4. Podstawowa biblioteka 2D i prosty interfejs okien.
5. Pierwszy sterownik dla wybranej, dobrze udokumentowanej rodziny GPU.
6. Aparat i podstawowe przechwytywanie obrazu.
7. Edytor zdjęć i Galeria.
8. Dźwięk, odtwarzacz filmów i dekodowanie programowe.
9. Akceleracja sprzętowa 2D/kompozycji, a później dekodowanie wideo.
10. Rozszerzanie obsługi kolejnych kart i rodzin urządzeń.

## 10. Zależności od innych części VGK

Grafika i multimedia zależą od wcześniejszego ukończenia:
- zarządzania pamięcią i ochrony dostępu do pamięci;
- obsługi PCI/PCIe i mapowania pamięci urządzeń;
- przerwań, timerów i synchronizacji;
- systemu plików i bezpiecznego zapisu;
- podstaw wejścia/wyjścia, w tym klawiatury i myszy;
- zarządzania procesami i uprawnieniami;
- infrastruktury aktualizacji i wycofywania sterowników.

Nie należy przyspieszać prac nad GPU kosztem tych fundamentów. Do czasu ich wdrożenia ten dokument pozostaje planem, a nie deklaracją gotowych funkcji.
