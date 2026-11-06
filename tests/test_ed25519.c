#include "ed25519.h"

#include <stdio.h>
#include <string.h>

static int from_hex(const char *hex, uint8_t *out, size_t bytes)
{
    for (size_t i = 0; i < bytes; ++i) {
        unsigned value = 0;
        if (sscanf(hex + i * 2, "%2x", &value) != 1) return 0;
        out[i] = (uint8_t)value;
    }
    return 1;
}

static int check(const char *name, VGK_ED25519_RESULT actual,
                 VGK_ED25519_RESULT expected)
{
    if (actual == expected) return 0;
    fprintf(stderr, "BLAD %s: wynik %d, oczekiwano %d\n",
            name, (int)actual, (int)expected);
    return 1;
}

int main(void)
{
    /* RFC 8032, section 7.1, test vector 1 (empty message). */
    uint8_t public_key[32], signature[64], changed_key[32], changed_sig[64];
    const char *pk_hex =
        "d75a980182b10ab7d54bfed3c964073a"
        "0ee172f3daa62325af021a68f707511a";
    const char *sig_hex =
        "e5564300c360ac729086e2cc806e828a"
        "84877f1eb8e5d974d873e06522490155"
        "5fb8821590a33bacc61e39701cf9b46b"
        "d25bf5f0595bbe24655141438e7a100b";
    int failures = 0;

    if (!from_hex(pk_hex, public_key, sizeof(public_key)) ||
        !from_hex(sig_hex, signature, sizeof(signature))) {
        fputs("BLAD: nie mozna odczytac wektora testowego\n", stderr);
        return 1;
    }

    failures += check("poprawny podpis RFC 8032",
        vgk_ed25519_verify(public_key, NULL, 0, signature),
        VGK_ED25519_VALID);

    memcpy(changed_sig, signature, sizeof(signature));
    changed_sig[0] ^= 1U;
    failures += check("zmieniony podpis",
        vgk_ed25519_verify(public_key, NULL, 0, changed_sig),
        VGK_ED25519_INVALID_SIGNATURE);

    memcpy(changed_key, public_key, sizeof(public_key));
    changed_key[0] ^= 1U;
    failures += check("zmieniony klucz publiczny",
        vgk_ed25519_verify(changed_key, NULL, 0, signature),
        VGK_ED25519_INVALID_SIGNATURE);

    failures += check("nieprawidlowy argument",
        vgk_ed25519_verify(NULL, NULL, 0, signature),
        VGK_ED25519_INVALID_ARGUMENT);
    failures += check("brak danych przy dodatniej dlugosci",
        vgk_ed25519_verify(public_key, NULL, 1, signature),
        VGK_ED25519_INVALID_ARGUMENT);

    if (failures != 0) return 1;
    puts("OK: testy Ed25519 RFC 8032 zakonczone powodzeniem.");
    return 0;
}
