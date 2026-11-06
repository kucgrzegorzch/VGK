#include "integrity.h"
#include "sha256.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const uint8_t *data;
    size_t length;
    size_t maximum_chunk;
    int fail;
} MEMORY_READER;

static int memory_read(void *opaque, uint64_t offset, uint8_t *buffer,
                       size_t capacity, size_t *bytes_read)
{
    MEMORY_READER *reader = (MEMORY_READER *)opaque;
    if (reader->fail) return -1;
    if (offset > reader->length) return -1;
    size_t remaining = reader->length - (size_t)offset;
    size_t count = remaining < capacity ? remaining : capacity;
    if (count > reader->maximum_chunk) count = reader->maximum_chunk;
    if (count != 0) memcpy(buffer, reader->data + (size_t)offset, count);
    *bytes_read = count;
    return 0;
}

static int check(const char *name, VGK_INTEGRITY_RESULT actual,
                 VGK_INTEGRITY_RESULT expected)
{
    if (actual == expected) return 0;
    fprintf(stderr, "BŁĄD %s: wynik %d, oczekiwano %d\n",
            name, (int)actual, (int)expected);
    return 1;
}

int main(void)
{
    static const uint8_t content[] = "VGK chroni pliki przed podmiana";
    uint8_t expected[32];
    uint8_t wrong[32];
    int failures = 0;

    vgk_sha256(content, sizeof(content) - 1, expected);
    memcpy(wrong, expected, sizeof(wrong));
    wrong[7] ^= 0x40;

    failures += check("zgodny bufor",
        vgk_verify_buffer_sha256(content, sizeof(content) - 1, expected),
        VGK_INTEGRITY_OK);
    failures += check("podmieniony bufor tej samej dlugosci",
        vgk_verify_buffer_sha256(content, sizeof(content) - 2, expected),
        VGK_INTEGRITY_HASH_MISMATCH);
    failures += check("bledny oczekiwany skrot",
        vgk_verify_buffer_sha256(content, sizeof(content) - 1, wrong),
        VGK_INTEGRITY_HASH_MISMATCH);
    failures += check("pusty wskaznik",
        vgk_verify_buffer_sha256(NULL, 1, expected),
        VGK_INTEGRITY_INVALID_ARGUMENT);

    MEMORY_READER reader = {
        .data = content,
        .length = sizeof(content) - 1,
        .maximum_chunk = 3,
        .fail = 0
    };
    failures += check("odczyt porcjami",
        vgk_verify_reader_sha256(memory_read, &reader, reader.length, expected),
        VGK_INTEGRITY_OK);

    reader.fail = 1;
    failures += check("blad odczytu",
        vgk_verify_reader_sha256(memory_read, &reader, reader.length, expected),
        VGK_INTEGRITY_READ_ERROR);

    reader.fail = 0;
    failures += check("niezgodny rozmiar",
        vgk_verify_reader_sha256(memory_read, &reader, reader.length + 1, expected),
        VGK_INTEGRITY_READ_ERROR);

    if (failures != 0) return 1;
    puts("OK: testy sprawdzania integralnosci zakonczone powodzeniem.");
    return 0;
}
