#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "uefi_memory_map.h"

#define LARGE_DESCRIPTOR_COUNT 13107U

typedef struct {
    VGK_EFI_MEMORY_DESCRIPTOR descriptor;
    uint64_t extension;
} TEST_DESCRIPTOR_STRIDE;

/*
 * 13,107 is the maximum number of minimum-size (40-byte) UEFI descriptors
 * that fit in the bootloader's 512 KiB firmware-map buffer.
 * Static storage avoids putting the stress-test buffers on the host stack.
 */
static VGK_EFI_MEMORY_DESCRIPTOR large_raw[LARGE_DESCRIPTOR_COUNT];
static VGK_MEMORY_MAP_ENTRY large_output[LARGE_DESCRIPTOR_COUNT];

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

static unsigned test_large_reverse_order_map(void)
{
    unsigned failures = 0;

    for (size_t i = 0; i < LARGE_DESCRIPTOR_COUNT; ++i) {
        size_t reverse_index = LARGE_DESCRIPTOR_COUNT - i;
        large_raw[i].type = 7;
        large_raw[i].physical_start = (uint64_t)reverse_index * UINT64_C(0x2000);
        large_raw[i].number_of_pages = 1;
        large_raw[i].attribute = 0;
        large_raw[i].reserved = 0;
    }

    {
        size_t count = 0;
        if (vgk_uefi_normalize_memory_map(
                large_raw, sizeof(large_raw),
                sizeof(VGK_EFI_MEMORY_DESCRIPTOR), 1,
                large_output, LARGE_DESCRIPTOR_COUNT, &count) != 0) {
            puts("FAIL: odrzucono duza odwrotnie uporzadkowana mape UEFI");
            return 1;
        }

        if (count != LARGE_DESCRIPTOR_COUNT) {
            puts("FAIL: niepoprawna liczba wpisow po sortowaniu duzej mapy");
            ++failures;
        }

        for (size_t i = 0; i < LARGE_DESCRIPTOR_COUNT; ++i) {
            uint64_t expected = (uint64_t)(i + 1U) * UINT64_C(0x2000);
            if (large_output[i].base != expected ||
                large_output[i].length != UINT64_C(4096) ||
                large_output[i].type != VGK_MEMORY_USABLE) {
                puts("FAIL: duza mapa UEFI nie zostala poprawnie posortowana");
                ++failures;
                break;
            }
        }
    }

    return failures;
}

int main(void)
{
    TEST_DESCRIPTOR_STRIDE raw[4];
    VGK_MEMORY_MAP_ENTRY output[4];
    size_t count = 0;
    unsigned failures = 0;

    put_descriptor(&raw[0], 7, UINT64_C(0x4000), 2, UINT64_C(0x8));
    put_descriptor(&raw[1], 9, UINT64_C(0x1000), 1, UINT64_C(0x1));
    put_descriptor(&raw[2], 11, UINT64_C(0x9000), 1, UINT64_C(0x2));
    put_descriptor(&raw[3], 12, UINT64_C(0xA000), 1, UINT64_C(0x2));

    if (vgk_uefi_normalize_memory_map(
            raw, sizeof(raw), sizeof(TEST_DESCRIPTOR_STRIDE), 1,
            output, 4, &count) != 0) {
        puts("FAIL: poprawna mapa firmware zostala odrzucona");
        ++failures;
    } else if (count != 4 ||
               output[0].base != UINT64_C(0x1000) ||
               output[0].type != VGK_MEMORY_ACPI_RECLAIM ||
               output[1].base != UINT64_C(0x4000) ||
               output[1].type != VGK_MEMORY_USABLE ||
               output[2].base != UINT64_C(0x9000) ||
               output[2].type != VGK_MEMORY_MMIO ||
               output[3].base != UINT64_C(0xA000) ||
               output[3].type != VGK_MEMORY_RESERVED) {
        puts("FAIL: niepoprawna normalizacja lub sortowanie mapy");
        ++failures;
    }

    count = 99;
    if (vgk_uefi_normalize_memory_map(
            raw, sizeof(raw), sizeof(TEST_DESCRIPTOR_STRIDE), 2,
            output, 4, &count) == 0 || count != 0) {
        puts("FAIL: zaakceptowano nieznana wersje deskryptora");
        ++failures;
    }

    count = 99;
    if (vgk_uefi_normalize_memory_map(
            raw, sizeof(raw), 32, 1, output, 4, &count) == 0 || count != 0) {
        puts("FAIL: zaakceptowano zbyt maly rozmiar deskryptora");
        ++failures;
    }

    count = 99;
    if (vgk_uefi_normalize_memory_map(
            raw, sizeof(raw), sizeof(TEST_DESCRIPTOR_STRIDE), 1,
            output, 3, &count) == 0 || count != 0) {
        puts("FAIL: zaakceptowano zbyt maly bufor wynikowy");
        ++failures;
    }

    put_descriptor(&raw[0], 7, UINT64_C(0x1000), 2, 0);
    put_descriptor(&raw[1], 7, UINT64_C(0x2000), 2, 0);
    if (vgk_uefi_normalize_memory_map(
            raw, 2 * sizeof(TEST_DESCRIPTOR_STRIDE),
            sizeof(TEST_DESCRIPTOR_STRIDE), 1, output, 4, &count) == 0) {
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

    put_descriptor(&raw[0], 7, UINT64_C(0x1001), 1, 0);
    count = 99;
    if (vgk_uefi_normalize_memory_map(
            raw, sizeof(TEST_DESCRIPTOR_STRIDE),
            sizeof(TEST_DESCRIPTOR_STRIDE), 1, output, 3, &count) == 0 ||
        count != 0) {
        puts("FAIL: zaakceptowano nie wyrownany poczatek zakresu UEFI");
        ++failures;
    }

    put_descriptor(&raw[0], 0, UINT64_C(0xB000), 1, 0);
    count = 0;
    if (vgk_uefi_normalize_memory_map(
            raw, sizeof(TEST_DESCRIPTOR_STRIDE),
            sizeof(TEST_DESCRIPTOR_STRIDE), 1, output, 4, &count) != 0 ||
        count != 1U || output[0].type != VGK_MEMORY_RESERVED) {
        puts("FAIL: EfiReservedMemoryType musi pozostac pamiecia RESERVED");
        ++failures;
    }

    failures += test_large_reverse_order_map();

    if (failures != 0)
        return 1;

    puts("OK: normalizacja mapy pamieci UEFI");
    return 0;
}
