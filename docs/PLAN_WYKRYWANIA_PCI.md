# Plan wykrywania urzadzen PCI i PCI Express w VGK

## Cel
VGK ma rozpoznawac kontrolery i urzadzenia komputera przez standardowe identyfikatory PCI. To warstwa inwentaryzacji sprzetu, a nie sterownik urzadzenia.

## Zasady bezpieczenstwa
- W pierwszym etapie tylko odczyt identyfikatorow i klas urzadzen.
- Bez zapisu do rejestrow konfiguracyjnych i bez zmiany BAR, MSI/MSI-X ani ustawien zasilania.
- Nie uruchamiac DMA ani nie mapowac pamieci urzadzen na tym etapie.
- Nieznane urzadzenie pozostaje nierozpoznane; nie probowac ladowac przypadkowego sterownika.
- Bledne lub niepelne dane sa odrzucane, a system zachowuje tryb diagnostyczny.
- Inwentaryzacja sprzetu ma byc oddzielona od decyzji o uruchomieniu sterownika.

## Kolejnosc wdrozenia
1. Znormalizowany rekord funkcji PCI: segment, magistrala, urządzenie, funkcja, identyfikatory producenta/urządzenia, rewizja, klasa, podklasa i interfejs programowy.
2. Walidacja rekordow i dekodowanie klas zgodnie ze standardem PCI.
3. Odczyt konfiguracji przez bezpieczny interfejs firmware UEFI PCI I/O przed ExitBootServices; później niezależna warstwa dostępu dla jądra.
4. Testy QEMU/OVMF z urzadzeniami wirtualnymi oraz testy blednych odpowiedzi firmware.
5. Wykrywanie funkcji PCI Express i odczyt lancucha mozliwosci dopiero po stabilnym odczycie bazowej przestrzeni konfiguracyjnej.
6. Dopiero potem baza identyfikatorow i dopasowanie kandydatow na sterowniki.

## Stan kodu
Dodano rekord VGK_PCI_DEVICE, walidacje numeru urzadzenia/funkcji i identyfikatora pustej funkcji oraz klasyfikacje klas: pamiec masowa, siec, obraz, multimedia, mostki, magistrale szeregowe i USB. Dodano testy jednostkowe tych reguł.

Dodano także skaner UEFI PCI I/O: pobiera uchwyty protokołu, odczytuje podstawowe 16 DWORD-ów przestrzeni konfiguracji oraz lokalizację segment/magistrala/urządzenie/funkcja. Ogranicza listę do 128 wpisów, zwalnia bufor uchwytów firmware i nie przekazuje uchwytów ani wskaźników do jądra. Inwentarz jest przekazywany w wersjonowanym protokole startowym v3. Błędy firmware nie blokują startu jądra i nie pozostawiają częściowej listy.

**Weryfikacja pełnego przepływu w CI/QEMU jest jeszcze w toku.** Nie ma testu na fizycznym komputerze ani sterowników kart graficznych, sieciowych i innych urządzeń. Odczyt BAR, mapowanie MMIO, DMA i obsługa PCI Express pozostają wyłączone do czasu osobnych etapów bezpieczeństwa.
