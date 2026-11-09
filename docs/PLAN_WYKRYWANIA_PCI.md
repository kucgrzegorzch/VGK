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
1. Znormalizowany rekord funkcji PCI: segment (w przyszlosci), magistrala, urzadzenie, funkcja, identyfikatory producenta/urzadzenia, rewizja, klasa, podklasa i interfejs programowy.
2. Walidacja rekordow i dekodowanie klas zgodnie ze standardem PCI.
3. Odczyt konfiguracji przez bezpieczny interfejs firmware UEFI PCI I/O przed ExitBootServices; pozniej niezalezna warstwa dostepu dla jadra.
4. Testy QEMU/OVMF z urzadzeniami wirtualnymi oraz testy blednych odpowiedzi firmware.
5. Wykrywanie funkcji PCI Express i odczyt lancucha mozliwosci dopiero po stabilnym odczycie bazowej przestrzeni konfiguracyjnej.
6. Dopiero potem baza identyfikatorow i dopasowanie kandydatow na sterowniki.

## Stan kodu
Dodano rekord VGK_PCI_DEVICE, walidacje numeru urzadzenia/funkcji i identyfikatora pustej funkcji oraz klasyfikacje klas: pamiec masowa, siec, obraz, multimedia, mostki, magistrale szeregowe i USB. Dodano testy jednostkowe tych reguł.

**Nie ma jeszcze enumeracji magistral z firmware ani odczytu konfiguracji PCI z rzeczywistego komputera.** Nie ma tez sterownikow kart graficznych, sieciowych ani innych urzadzen. Ten etap przygotowuje bezpieczny format danych i reguly rozpoznawania.
