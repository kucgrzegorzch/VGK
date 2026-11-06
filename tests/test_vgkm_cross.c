#include "manifest.h"

#include <stdio.h>

static int read_file(const char *path, unsigned char *buffer, size_t capacity,
                     size_t *length)
{
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    size_t n = fread(buffer, 1, capacity, file);
    int extra = fgetc(file);
    int failed = ferror(file);
    fclose(file);
    if (failed || extra != EOF) return -1;
    *length = n;
    return 0;
}

int main(int argc, char **argv)
{
    unsigned char encoded[VGK_MANIFEST_MAX_SIZE], signature[64];
    unsigned char public_der[128], public_key[32];
    size_t encoded_size = 0, signature_size = 0, public_size = 0;
    VGK_MANIFEST manifest;

    if (argc != 4 ||
        read_file(argv[1], encoded, sizeof(encoded), &encoded_size) ||
        read_file(argv[2], signature, sizeof(signature), &signature_size) ||
        read_file(argv[3], public_der, sizeof(public_der), &public_size) ||
        signature_size != sizeof(signature) || public_size < sizeof(public_key))
        return 2;

    for (size_t i = 0; i < sizeof(public_key); ++i)
        public_key[i] = public_der[public_size - sizeof(public_key) + i];

    VGK_MANIFEST_RESULT result = vgk_manifest_open(
        &manifest, encoded, encoded_size, public_key, signature);
    if (result != VGK_MANIFEST_OK) {
        fprintf(stderr, "Parser C odrzucił wykaz: %d\n", (int)result);
        return 1;
    }
    puts("OK: parser C zaakceptował wykaz utworzony przez narzędzia Python.");
    return 0;
}
