# Lista kontrolna aktywacji GDT, TSS i IDT w VGK

## Status

Dokument opisuje warunki, które muszą zostać spełnione przed aktywacją deskryptorów procesora w normalnym jądrze VGK. Nie włącza żadnej funkcji i nie jest dowodem gotowości do uruchomienia na fizycznym komputerze.

Stan bieżący:
- kod potrafi przygotować GDT, 64-bitowy TSS, stosy IST, 32 bramy wyjątków i obraz IDTR;
- kontrolowane obrazy testowe QEMU osobno wykonują LGDT, LTR i LIDT w celu sprawdzenia #PF/#DF;
- normalna ścieżka jądra nie wykonuje LGDT, LTR, LIDT ani zmiany CR3;
- testy QEMU nie zastępują testu na fizycznym sprzęcie.

## Warunki blokujące aktywację

1. **GDT i selektory**
   - GDT musi być kompletna, wyrównana i dostępna przez cały okres używania.
   - Obecne selektory VGK: kod 0x08, dane 0x10, TSS 0x18.
   - Po załadowaniu nowej GDT nie wolno zakładać, że rejestry segmentowe odziedziczone po firmware wskazują na jej wpisy. Należy jawnie przeładować CS przez kontrolowany daleki transfer sterowania oraz ustawić DS/ES/SS na prawidłowy selektor danych.
   - Kod musi działać na nowym selektorze CS, zanim zostanie wykonana dalsza inicjalizacja zależna od GDT.

2. **TSS i stosy IST**
   - TSS musi być poprawnie wyrównany, a jego pełny zakres musi mieścić się w kanonicznym obszarze adresowym.
   - Deskryptor TSS musi wskazywać dokładnie ten obiekt i mieć limit równy rozmiarowi TSS minus jeden.
   - Wszystkie używane wpisy IST muszą wskazywać na istniejące, rozłączne i zweryfikowane stosy.
   - TSS należy wypełnić przed instalacją IDT, której bramy odwołują się do IST.
   - LTR wykonuje się dopiero po załadowaniu właściwej GDT. Procesor oznacza deskryptor TSS jako zajęty; ponowne ładowanie TR wymaga osobnego, świadomego postępowania z tym stanem.
   - RSP0 pozostaje obecnie zerowy. Przed jakimkolwiek przejściem do CPL3 trzeba przygotować prawidłowy, kanoniczny i wyrównany stos RSP0. Do tego czasu VGK nie może uruchamiać kodu użytkownika.

3. **IDT i przerwania**
   - Obecna tablica przygotowuje 32 wpisy dla wektorów wyjątków 0–31. Jej limit IDTR wynosi 511 bajtów. Nie jest to pełna 256-wpisowa tablica przerwań x86-64.
   - Te 32 wpisy wystarczają do obecnego zakresu wyjątków procesora, ale nie stanowią gotowej tablicy dla wszystkich wektorów zewnętrznych IRQ.
   - Nie wolno włączać przerwań maskowalnych instrukcją STI, dopóki VGK nie przygotuje pełnej tablicy 256 wpisów albo równoważnej, formalnie sprawdzonej konfiguracji obejmującej wszystkie możliwe wektory, oraz nie skonfiguruje i nie zamaskuje/obsłuży kontrolerów PIC/APIC.
   - IF ma pozostać wyzerowany podczas wstępnej aktywacji. Źródła NMI i błędu kontroli maszyny wymagają osobnego uwzględnienia, ponieważ CLI nie blokuje NMI ani #MC.
   - Każda brama musi mieć prawidłowy selektor kodu, kanoniczny adres procedury, poprawny typ/atrybuty, wyzerowane pole zarezerwowane oraz właściwy numer IST.

## Zalecana kolejność przyszłej aktywacji

Kolejność jest projektem do osobnej implementacji i przeglądu; nie należy kopiować jej do normalnego jądra bez testów:

1. Zatrzymać przerwania maskowalne (CLI) i potwierdzić, że środowisko nie polega na ich obsłudze.
2. Sprawdzić wszystkie zakresy GDT, TSS, stosów IST, procedur wejściowych i tablicy IDT przed wykonaniem instrukcji uprzywilejowanych.
3. Załadować GDT (LGDT).
4. Wykonać kontrolowany daleki transfer sterowania, aby przeładować CS na selektor VGK 0x08; następnie ustawić segmenty danych na selektor VGK 0x10.
5. Załadować TR (LTR) selektorem TSS 0x18, po wcześniejszym sprawdzeniu deskryptora i pełnego obiektu TSS.
6. Załadować IDT (LIDT) dopiero po sprawdzeniu wszystkich bram i gotowości wszystkich stosów, do których prowadzą wpisy IST.
7. Pozostawić IF=0. Wykonać testy negatywne i kontrolowane wyjątki w osobnym obrazie testowym, potwierdzając diagnostykę oraz bezpieczne zatrzymanie.
8. Dopiero w osobnym etapie przygotować 256 wpisów IDT, procedury IRQ, konfigurację PIC/APIC i obsługę NMI/#MC. STI może pojawić się dopiero po sprawdzeniu całego przepływu i polityki maskowania.

## Kryteria przed zmianą normalnej ścieżki jądra

- Wszystkie testy jednostkowe GDT/TSS/IDT/IDTR/IST przechodzą.
- Testy asemblerowych stubów potwierdzają zgodność ramek i zachowanie rejestrów.
- QEMU/OVMF potwierdza kontrolowane #PF i #DF oraz ścieżki błędne bez niekontrolowanego odczytu ramek.
- Testy sprawdzają, że nieprawidłowy GDT/TSS/IST/IDT zatrzymuje inicjalizację przed odpowiednią instrukcją uprzywilejowaną.
- Osobny przegląd potwierdza kolejność przeładowania CS/segmentów, LTR i LIDT.
- Przed włączeniem IRQ istnieje pełna, zweryfikowana polityka 256 wektorów i konfiguracja kontrolera przerwań.
- Przed uruchomieniem CPL3 istnieje niezerowy, prawidłowy RSP0 i przetestowane przejście uprawnień.
- Normalna ścieżka pozostaje nieaktywna, dopóki powyższe warunki nie zostaną spełnione.

## Zasada nadrzędna

Sukces testu QEMU, w którym osobny obraz testowy ładuje GDT/TSS/IDT, nie upoważnia do automatycznego włączenia tych instrukcji w normalnym jądrze. Aktywacja musi być osobną, małą zmianą z własnym CI, testami negatywnymi i możliwością bezpiecznego wycofania.
