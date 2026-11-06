# Binarny wykaz bezpieczeństwa VGKM

Ten dokument opisuje format odczytywany przez `boot/crypto/manifest.c`.
Wydanie przeznaczone do uruchomienia musi używać tego samego formatu.
Nie wolno tworzyć jednego wykazu JSON dla narzędzi wydania i drugiego,
binarnego wykazu dla programu uruchamiającego.

## Pliki wydania

- `VGKM` — binarny wykaz opisany poniżej.
- `VGKM.sig` — dokładnie 64 bajty podpisu Ed25519.
- Klucz publiczny wydawcy — zaufany klucz wbudowany lub dostarczony
  przez osobny, jawnie zaufany mechanizm konfiguracji.
- Klucz prywatny — pozostaje wyłącznie u wydawcy; nigdy nie trafia do repozytorium,
  obrazu ani archiwum wydania.

Podpis obejmuje dokładnie wszystkie bajty pliku `VGKM`, bez dopisywania
znaku końca wiersza i bez przekształcania danych.

## Kodowanie

Wszystkie liczby całkowite są zapisane w kolejności little-endian.

### Nagłówek — 8 bajtów

| Przesunięcie | Długość | Znaczenie |
|---:|---:|---|
| 0 | 4 | Stałe bajty ASCII `VGKM` |
| 4 | 2 | Wersja formatu, obecnie `1` |
| 6 | 2 | Liczba wpisów: od 1 do 128 |

### Każdy wpis

Wpisy następują bez przerw, jeden po drugim.

| Pole | Długość | Znaczenie |
|---|---:|---|
| Długość ścieżki | 2 bajty | Liczba bajtów ścieżki |
| Rozmiar pliku | 8 bajtów | Rozmiar pliku w bajtach |
| Skrót | 32 bajty | SHA-256 całego pliku |
| Ścieżka | zmienna | Ścieżka względna w ASCII |

Stała część wpisu ma 42 bajty. Ścieżka może mieć maksymalnie 240 bajtów.
Cały wykaz może mieć maksymalnie 65536 bajtów.

## Zasady ścieżek

- Ścieżki są względne wobec głównego katalogu wydania.
- Separatorem w wykazie jest ukośnik `/`; czytnik UEFI zamienia go na `\\`.
- Dozwolone są wyłącznie litery ASCII, cyfry, kropka, podkreślenie i myślnik
  w nazwach składników ścieżki.
- Nie wolno używać pustych składników, `.`, `..`, ścieżek bezwzględnych,
  ukośnika odwrotnego, dwukropka ani powtórzonych ścieżek.
- Generator powinien odrzucać dowiązania symboliczne i pliki spoza katalogu wydania.
- Dla powtarzalnych wydań wpisy są sortowane rosnąco według bajtów ścieżki.

## Wymagana kolejność kontroli

1. Ograniczyć rozmiar wykazu i odczytać jego bajty bez interpretowania ścieżek.
2. Sprawdzić podpis Ed25519 na dokładnych bajtach wykazu.
3. Sprawdzić nagłówek, wersję, liczbę wpisów, granice danych i ścieżki.
4. Dla każdego wpisu otworzyć wskazany plik przez bezpieczny czytnik.
5. Sprawdzić dokładny rozmiar i SHA-256; każda rozbieżność oznacza odmowę uruchomienia.
6. Dopiero po pozytywnym wyniku wszystkich kontroli wolno przejść do ładowania jądra.

## Zasada zgodności

Generator wydania, narzędzie kontrolne na komputerze wydawcy, parser C,
testy i program uruchamiający muszą korzystać z jednego formatu VGKM.
JSON może służyć do raportów lub pomocniczych danych, ale nie może zastępować
binarnego wykazu przekazywanego do parsera VGK.
