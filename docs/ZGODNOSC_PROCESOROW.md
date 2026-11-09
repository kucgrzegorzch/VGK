# Zgodność procesorów i bezpieczne wykorzystanie ich możliwości w VGK

Status: plan techniczny oparty na aktualnym module CPUID; nie jest deklaracją pełnej obsługi wszystkich procesorów.

## 1. Zasada nadrzędna

VGK rozpoznaje możliwości procesora, a nie dopuszcza lub odrzuca komputer wyłącznie na podstawie nazwy modelu. Identyfikacja producenta, rodziny, modelu i rewizji służy diagnostyce oraz ewentualnemu stosowaniu znanych poprawek. O dostępności funkcji decydują wykryte możliwości i stan konfiguracji procesora.

Nie wolno wykonywać instrukcji rozszerzenia tylko dlatego, że procesor je deklaruje. System musi sprawdzić również, czy przygotował odpowiedni stan i obsługę po stronie systemu operacyjnego.

## 2. Warstwy zgodności

### Warstwa podstawowa x86-64

- Kod jądra jest kompilowany dla wspólnego, konserwatywnego zestawu instrukcji x86-64.
- Nie wolno kompilować całego jądra z opcją zakładającą AVX, AVX2 ani konkretne rozszerzenie producenta.
- Obecne ustawienia kompilacji wyłączają automatyczne użycie MMX/SSE/SSE2 w kodzie jądra. To bezpieczny punkt wyjścia, dopóki VGK nie ma pełnego zarządzania stanem jednostek zmiennoprzecinkowych i wektorowych.
- SSE2 i inne rozszerzenia są wykrywane, ale samo wykrycie nie oznacza, że jądro już z nich korzysta.

### Ochrona pamięci

- Obecny etap startowy wymaga potwierdzonej obsługi NX i prawidłowej szerokości adresu fizycznego.
- Jeżeli NX nie jest dostępne, obecny kod nie powinien uruchamiać ochrony pamięci tak, jakby NX działało. Ma zgłosić czytelny powód odmowy.
- Nie należy osłabiać ochrony pamięci tylko po to, aby uruchomić VGK na starszym procesorze. Ewentualny przyszły tryb zgodności musi być osobno zaprojektowany, ograniczony i jawnie oznaczony.

### Rozszerzenia wektorowe

- SSE, SSE2, SSE3, SSSE3, SSE4.1 i SSE4.2 są zapisywane jako osobne możliwości sprzętowe.
- AVX można uznać za użyteczne dopiero po potwierdzeniu bitu AVX, OSXSAVE oraz aktywowania w XCR0 zarówno stanu XMM, jak i YMM.
- AVX2 wymaga dodatkowo odpowiedniego bitu w CPUID liść 7 oraz wszystkich warunków AVX.
- Samo CPUID nie wystarcza. Zanim jakikolwiek proces lub sterownik będzie wykonywał instrukcje wektorowe, VGK musi umieć bezpiecznie zachować i odtworzyć stan procesora podczas przełączenia zadań, obsłużyć wyjątki i kontrolować użycie tych instrukcji.
- Do tego czasu rozszerzenia pozostają informacją diagnostyczną; kompilacja podstawowego jądra nie może ich wymagać.

## 3. Wielu producentów i nowe generacje

- Nie stosujemy listy dozwolonych modeli Intel lub AMD.
- Nieznany model z poprawnie wykrytym zestawem wymaganych możliwości może używać warstwy podstawowej.
- Funkcje opcjonalne włączane są niezależnie, po sprawdzeniu warunków sprzętowych i systemowych.
- Nieznany bit lub nieobsługiwany liść CPUID jest traktowany jako brak potwierdzenia funkcji, a nie jako jej obecność.
- Informacje o znanych błędach procesorów (errata) mogą w przyszłości ograniczać konkretną funkcję, ale baza takich wyjątków musi być wersjonowana, podpisana i możliwa do wycofania.
- Aktualizacja mikrokodu, jeśli zostanie dodana, będzie osobnym mechanizmem z kontrolą producenta, platformy, integralności i możliwości bezpiecznego powrotu. Nie może być warunkiem uruchomienia podstawowego systemu.

## 4. Kolejność wdrożenia

1. Utrzymać bezpieczny wspólny zestaw instrukcji dla jądra.
2. Rozbudować testy dekodowania CPUID o brakujące liście, nieznane bity i graniczne wartości.
3. Rozdzielić w interfejsie diagnostycznym trzy stany: sprzęt obsługuje, system przygotował, funkcja jest faktycznie używana.
4. Dodać własne tablice stron i egzekwowanie uprawnień pamięci, w tym NX, po wcześniejszym sprawdzeniu wymaganych możliwości.
5. Dodać obsługę wyjątków i przerwań.
6. Zbudować mechanizm zachowania stanu FPU/SSE/AVX przy przełączaniu zadań.
7. Dopiero po testach włączyć dobieranie zoptymalizowanych ścieżek dla wybranych operacji.
8. Testować na emulatorach różnych konfiguracji CPUID, a następnie na fizycznych komputerach Intel i AMD z różnych generacji.

## 5. Wymagane testy

- procesor bez wymaganego NX: bezpieczna odmowa uruchomienia obecnego profilu jądra;
- procesor z NX i różną szerokością adresu fizycznego: prawidłowe wykrycie lub odmowa poza obsługiwanym zakresem;
- brak liścia CPUID 7: AVX2 nie może zostać uznane za dostępne;
- AVX bez OSXSAVE: AVX nie może być uznane za użyteczne i nie wolno wykonywać XGETBV;
- AVX i OSXSAVE, ale bez wymaganych bitów XCR0: AVX nie może być uznane za użyteczne;
- AVX2 bez poprawnie przygotowanego stanu AVX: AVX2 nie może być użyteczne;
- nieznany producent/model: brak automatycznej odmowy, jeśli wymagane możliwości są obecne;
- przełączenie zadań: po wdrożeniu obsługi stanu wektorowego żadne zadanie nie może odziedziczyć rejestrów innego zadania;
- porównanie kompilacji podstawowej: kod nie może przypadkowo zawierać instrukcji niedostępnych w przyjętej warstwie bazowej.

## 6. Stan obecny i ograniczenia

W gałęzi roboczej istnieje już moduł CPUID, który rozpoznaje producenta, rodzinę, model, stepping, NX, szerokość adresu fizycznego oraz wybrane rozszerzenia SSE/AVX/AVX2. Rozróżnia deklarację sprzętową AVX od dostępności stanu AVX w systemie. Jądro nadal używa bezpiecznego zestawu podstawowego.

Nie ma jeszcze pełnego zarządzania FPU/SSE/AVX, przełączania zadań ani tablic stron egzekwujących uprawnienia segmentów. Dlatego nie deklarujemy jeszcze pełnej obsługi optymalizacji wektorowych ani pełnej ochrony NX w działającym jądrze.
