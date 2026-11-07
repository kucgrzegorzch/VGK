#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "uefi_memory_map.h"

typedef struct {
    VGK_EFI_MEMORY_DESCRIPTOR descriptor;
    uint64_t extension;
} TEST_DESCRIPTOR_STRIDE;

static void put_descriptor(
    TEST_DESCRIPTOR_STRIDE *slot,
    uint32_t type,
    uint64_t base,
    uint64_t pages,
    uint64_t attributes)
{
    memset(slot, 0, sizeof(*slot));
    slot->descriptor.type = type;
    slot->descriptor.physical_start = base;
    slot->descriptor.number_of_pages = pages;
    slot->descriptor.attribute = attributes;
    slot->extension = UINT64_C(0xA5A5A5A5A5A5A5A5);
}

int main(void)
{
    TEST_DESCRIPTOR_STRIDE raw[3];
    VGK_MEMORY_MAP_ENTRY output[3];
    size_t count = 0;
    unsigned failures = 0;

    put_descriptor(&raw[0], 7, UINT64_C(0x4000), 2, UINT64_C(0x8));
    put_descriptor(&raw[1], 9, UINT64_C(0x1000), 1, UINT64_C(0x1));
    put_descriptor(&raw[2], 11, UINT64_C(0x9000), 1, UINT64_C(0x2));

    if (vgk_uefi_normalize_memory_map(
            raw, sizeof(raw), sizeof(TEST_DESCRIPTOR_STRIDE), 1,
            output, 3, &count) != 0) {
        puts("FAIL: poprawna mapa firmware zostala odrzucona");
        ++failures;
    } else if (count != 3 ||
               output[0].base != UINT64_C(0x1000) ||
               output[0].type != VGK_MEMORY_ACPI_RECLAIM ||
               output[1].base != UINT64_C(0x4000) ||
               output[1].type != VGK_MEMORY_USABLE ||
               output[2].base != UINT64_C(0x9000) ||
               output[2].type != VGK_MEMORY_RESERVED) {
        puts("FAIL: niepoprawna normalizacja lub sortowanie mapy");
        ++failures;
    }

    count = 99;
    if (vgk_uefi_normalize_memory_map(
            raw, sizeof(raw), sizeof(TEST_DESCRIPTOR_STRIDE), 2,
            output, 3, &count) == 0 || count != 0) {
        puts("FAIL: zaakceptowano nieznana wersje deskryptora");
        ++failures;
    }

    count = 99;
    if (vgk_uefi_normalize_memory_map(
            raw, sizeof(raw), 32, 1, output, 3, &count) == 0 || count != 0) {
        puts("FAIL: zaakceptowano zbyt maly rozmiar deskryptora");
        ++failures;
    }

    count = 99;
    if (vgk_uefi_normalize_memory_map(
            raw, sizeof(raw), sizeof(TEST_DESCRIPTOR_STRIDE), 1,
            output, 2, &count) == 0 || count != 0) {
        puts("FAIL: zaakceptowano zbyt maly bufor wynikowy");
        ++failures;
    }

    put_descriptor(&raw[0], 7, UINT64_C(0x1000), 2, 0);
    put_descriptor(&raw[1], 7, UINT64_C(0x2000), 2, 0);
    if (vgk_uefi_normalize_memory_map(
            raw, 2 * sizeof(TEST_DESCRIPTOR_STRIDE),
            sizeof(TEST_DESCRIPTOR_STRIDE), 1, output, 3, &count) == 0) {
        puts("FAIL: zaakceptowano nakladajace sie zakresy");
        ++failures;
    }

    put_descriptor(&raw[0], 7, UINT64_MAX - UINT64_C(0xFFF), 2, 0);
    if (vgk_uefi_normalize_memory_map(
            raw, sizeof(TEST_DESCRIPTOR_STRIDE),
            sizeof(TEST_DESCRIPTOR_STRIDE), 1, output, 3, &count) == 0) {
        puts("FAIL: zaakceptowano przepełnienie adresu fizycznego");
        ++failures;
    }

    put_descriptor(&raw[0], 7, UINT64_C(0x1000), 0, 0);
    if (vgk_uefi_normalize_memory_map(
            raw, sizeof(TEST_DESCRIPTOR_STRIDE),
            sizeof(TEST_DESCRIPTOR_STRIDE), 1, output, 3, &count) == 0) {
        puts("FAIL: zaakceptowano deskryptor o zerowej liczbie stron");
        ++failures;
    }

    if (failures != 0)
        return 1;

    puts("OK: normalizacja mapy pamieci UEFI");
    return 0;
}
