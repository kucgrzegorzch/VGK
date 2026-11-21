# Ustawienia domyślne — założenie projektowe VGK

**Status:** decyzja projektowa do dalszej implementacji; dokument nie wprowadza jeszcze funkcji resetowania.

## Nazewnictwo interfejsu

W całym systemie należy stosować tę samą, krótką nazwę przycisku:

**„Przywróć domyślnie”**

Nie używać określenia „ustawienia fabryczne” jako etykiety przycisku. Chodzi o przywrócenie domyślnej konfiguracji samego systemu VGK.

## Zakres działania

1. **Ustawienia główne:** przycisk „Przywróć domyślnie” przywraca wszystkie objęte tą funkcją ustawienia systemowe do wartości początkowych, odpowiadających konfiguracji VGK po instalacji.
2. **Ustawienia dokładne:** przycisk o tej samej nazwie przywraca wartości domyślne wyłącznie w aktualnie wybranej sekcji. Pozostałe sekcje pozostają bez zmian.
3. **Pojedynczy parametr:** docelowo można udostępnić przywrócenie domyślnej wartości pojedynczego ustawienia, bez resetowania całej sekcji.

## Ochrona danych i bezpieczeństwo

- Przywracanie ustawień nie może usuwać dokumentów, zdjęć ani innych plików użytkownika.
- Nie może odinstalowywać programów ani usuwać danych użytkownika.
- Nie może zmieniać identyfikacji sprzętu, danych firmware ani krytycznych parametrów rozruchowych w sposób, który mógłby uniemożliwić uruchomienie systemu.
- Przed wykonaniem operacji system musi jasno pokazać zakres resetu i poprosić o potwierdzenie.
- Po zakończeniu system powinien poinformować, które ustawienia przywrócono; w razie błędu nie może pozostawić konfiguracji w nieokreślonym, częściowo zmienionym stanie.

## Zasady implementacji do ustalenia

- Wartości domyślne muszą mieć jednoznaczne, wersjonowane źródło powiązane z wersją VGK.
- Reset całego systemu i reset pojedynczej sekcji muszą mieć wyraźnie rozdzielone zakresy.
- Operacja powinna być odporna na przerwanie i błędy: najlepiej przygotować nową konfigurację, zweryfikować ją, a następnie zatwierdzić atomowo.
- Ustawienia sprzętowe i bezpieczeństwa wymagają osobnej klasyfikacji; nie wolno bezwarunkowo nadpisywać wartości wykrywanych lub wymaganych przez platformę.
- Szczegółowy katalog ustawień objętych resetem, obsługa kopii zapasowej oraz zachowanie po aktualizacji systemu pozostają do zaprojektowania przed implementacją.

## Etap

Na obecnym etapie jest to wymaganie architektoniczno-interfejsowe. Nie dodaje jeszcze przycisków, kodu resetowania ani nie zmienia zachowania działającego jądra.
