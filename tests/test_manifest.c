#include "manifest.h"
#include "monocypher/monocypher-ed25519.h"

#include <stdio.h>
#include <string.h>

static int check(const char *name, VGK_MANIFEST_RESULT actual,
                 VGK_MANIFEST_RESULT expected)
{
    if (actual == expected) return 0;
    fprintf(stderr, "BLAD %s: wynik %d, oczekiwano %d\n",
            name, (int)actual, (int)expected);
    return 1;
}

static size_t make_manifest(uint8_t *out, const char *path)
{
    static const uint8_t digest[32] = {
        0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,
        0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,
        0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad
    };
    size_t path_length = strlen(path);
    size_t size = 8U + 42U + path_length;
    memset(out, 0, size);
    out[0]='V'; out[1]='G'; out[2]='K'; out[3]='M';
    out[4]=1; out[5]=0; out[6]=1; out[7]=0;
    out[8]=(uint8_t)path_length; out[9]=0;
    out[10]=3; /* file size = 3 bytes */
    memcpy(out + 8 + 10, digest, sizeof(digest));
    memcpy(out + 8 + 42, path, path_length);
    return size;
}

int main(void)
{
    uint8_t seed[32] = {1};
    uint8_t secret_key[64], public_key[32], signature[64];
    uint8_t encoded[512], altered[512];
    VGK_MANIFEST manifest;
    VGK_MANIFEST_ENTRY entry;
    int failures = 0;

    crypto_ed25519_key_pair(secret_key, public_key, seed);
    size_t length = make_manifest(encoded, "boot/kernel.elf");
    crypto_ed25519_sign(signature, secret_key, encoded, length);

    failures += check("poprawny podpisany wykaz",
        vgk_manifest_open(&manifest, encoded, length, public_key, signature),
        VGK_MANIFEST_OK);
    failures += check("odczyt wpisu",
        vgk_manifest_get(&manifest, 0, &entry), VGK_MANIFEST_OK);
    if (entry.path_length != strlen("boot/kernel.elf") ||
        memcmp(entry.path, "boot/kernel.elf", entry.path_length) != 0 ||
        entry.file_size != 3U) {
        fputs("BLAD: zawartosc wpisu jest niepoprawna\n", stderr);
        ++failures;
    }

    memcpy(altered, encoded, length);
    altered[length - 1] ^= 1U;
    failures += check("zmieniony wykaz",
        vgk_manifest_open(&manifest, altered, length, public_key, signature),
        VGK_MANIFEST_BAD_SIGNATURE);

    length = make_manifest(encoded, "../kernel.elf");
    crypto_ed25519_sign(signature, secret_key, encoded, length);
    failures += check("sciezka wychodzaca poza katalog",
        vgk_manifest_open(&manifest, encoded, length, public_key, signature),
        VGK_MANIFEST_BAD_FORMAT);

    length = make_manifest(encoded, "boot//kernel.elf");
    crypto_ed25519_sign(signature, secret_key, encoded, length);
    failures += check("pusty element sciezki",
        vgk_manifest_open(&manifest, encoded, length, public_key, signature),
        VGK_MANIFEST_BAD_FORMAT);

    length = make_manifest(encoded, "boot/kernel.elf");
    encoded[length++] = 0;
    crypto_ed25519_sign(signature, secret_key, encoded, length);
    failures += check("nadmiarowe dane",
        vgk_manifest_open(&manifest, encoded, length, public_key, signature),
        VGK_MANIFEST_BAD_FORMAT);

    if (failures != 0) return 1;
    puts("OK: testy podpisanego wykazu VGK zakonczone powodzeniem.");
    return 0;
}
