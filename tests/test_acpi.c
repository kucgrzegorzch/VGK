#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vgk_acpi.h"

#define EXPECT(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static VGK_MEMORY_MAP_ENTRY memory_map[4];
static uint8_t rsdp[4096];
static unsigned failures;

static void set_checksum(uint8_t *bytes, size_t length, size_t checksum_offset)
{
    uint8_t sum = 0;
    bytes[checksum_offset] = 0;
    for (size_t i = 0; i < length; ++i)
        sum = (uint8_t)(sum + bytes[i]);
    bytes[checksum_offset] = (uint8_t)(0U - sum);
}

static void make_rsdp(uint8_t revision, uint32_t length)
{
    memset(rsdp, 0, sizeof(rsdp));
    memcpy(rsdp, "RSD PTR ", 8);
    rsdp[15] = revision;
    rsdp[20] = (uint8_t)length;
    rsdp[21] = (uint8_t)(length >> 8);
    rsdp[22] = (uint8_t)(length >> 16);
    rsdp[23] = (uint8_t)(length >> 24);
    if (revision >= 2U && length >= 36U && length <= sizeof(rsdp)) {
        set_checksum(rsdp, 20U, 8U);
        set_checksum(rsdp, length, 32U);
    } else {
        set_checksum(rsdp, 20U, 8U);
    }
}

static int validate(uint64_t address, uint64_t *length)
{
    return vgk_acpi_rsdp_validate(
        memory_map, sizeof(memory_map) / sizeof(memory_map[0]),
        address, rsdp, length);
}

int main(void)
{
    uint64_t length = UINT64_MAX;

    memory_map[0] = (VGK_MEMORY_MAP_ENTRY){
        UINT64_C(0x1000), UINT64_C(0x1000), VGK_MEMORY_ACPI_RECLAIM, 0};
    memory_map[1] = (VGK_MEMORY_MAP_ENTRY){
        UINT64_C(0x2000), UINT64_C(0x1000), VGK_MEMORY_ACPI_NVS, 0};
    memory_map[2] = (VGK_MEMORY_MAP_ENTRY){
        UINT64_C(0x3000), UINT64_C(0x1000), VGK_MEMORY_RESERVED, 0};
    memory_map[3] = (VGK_MEMORY_MAP_ENTRY){
        UINT64_C(0x5000), UINT64_C(0x1000), VGK_MEMORY_FIRMWARE, 0};

    make_rsdp(0U, 20U);
    EXPECT(validate(UINT64_C(0x1800), &length));
    EXPECT(length == 20U);

    make_rsdp(2U, 36U);
    EXPECT(validate(UINT64_C(0x1FF0), &length));
    EXPECT(length == 36U);

    make_rsdp(2U, 36U);
    EXPECT(!validate(UINT64_C(0x2FF0), &length));
    EXPECT(length == 0U);

    make_rsdp(2U, 36U);
    EXPECT(!validate(UINT64_C(0x4000), &length));

    make_rsdp(2U, 36U);
    EXPECT(validate(UINT64_C(0x5000), &length));

    make_rsdp(2U, 36U);
    rsdp[0] = 'X';
    EXPECT(!validate(UINT64_C(0x1800), &length));

    make_rsdp(2U, 36U);
    rsdp[8] ^= 1U;
    EXPECT(!validate(UINT64_C(0x1800), &length));

    make_rsdp(2U, 36U);
    rsdp[35] ^= 1U;
    EXPECT(!validate(UINT64_C(0x1800), &length));

    make_rsdp(2U, 35U);
    EXPECT(!validate(UINT64_C(0x1800), &length));

    make_rsdp(2U, 4097U);
    EXPECT(!validate(UINT64_C(0x1800), &length));

    make_rsdp(2U, 36U);
    EXPECT(!validate(0U, &length));
    EXPECT(!validate(UINT64_MAX - 10U, &length));

    make_rsdp(2U, 36U);
    EXPECT(!vgk_acpi_rsdp_validate(
        memory_map, 0U, UINT64_C(0x1800), rsdp, &length));

    if (failures != 0) {
        fprintf(stderr, "VGK ACPI RSDP: %u test(s) failed\n", failures);
        return 1;
    }

    puts("OK: ACPI RSDP – zakresy pamięci, checksum i granice długości.");
    return 0;
}
