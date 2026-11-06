# Pochodzenie i licencja modułu Ed25519

VGK korzysta z biblioteki kryptograficznej Monocypher w wersji 4.0.3, pobranej z oficjalnego repozytorium projektu:

- Projekt: https://github.com/LoupVaillant/Monocypher
- Wersja: 4.0.3
- Licencja: BSD-2-Clause lub CC0-1.0 (zgodnie z nagłówkami plików źródłowych).
- Oryginalne pliki licencji zachowano w katalogu `boot/crypto/monocypher/`.

W VGK umieszczono:
- `monocypher.c` i `monocypher.h`;
- `monocypher-ed25519.c` i `monocypher-ed25519.h`.

Pliki źródłowe zachowują oryginalne informacje o autorach i warunkach licencji. Moduł `boot/crypto/ed25519.c` jest własnym, niewielkim interfejsem VGK wywołującym funkcję weryfikacji biblioteki.

Nie jest to kryptografia napisana od zera przez VGK. Wybrano istniejącą implementację zamiast tworzyć własny algorytm podpisu bez odpowiedniego audytu. Przed wydaniem należy zachować niniejszą informację i warunki licencji w dokumentacji oraz materiałach dystrybucyjnych.
