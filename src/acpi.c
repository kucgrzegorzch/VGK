#include "vgk_acpi.h"

#define VGK_ACPI_RSDP_MIN_LENGTH UINT64_C(20)
#define VGK_ACPI_RSDP_EXTENDED_PREFIX UINT64_C(24)
#define VGK_ACPI_RSDP_MAX_LENGTH UINT64_C(4096)

static int acpi_memory_type_is_readable(uint64_t type)
{
    return type == VGK_MEMORY_ACPI_RECLAIM ||
           type == VGK_MEMORY_ACPI_NVS ||
           type == VGK_MEMORY_FIRMWARE;
}

/*
 * Require complete coverage, including adjacent descriptors. Gaps,
 * arithmetic overflow, generic reserved memory and MMIO are rejected.
 */
static int range_is_readable_acpi_memory(
    const VGK_MEMORY_MAP_ENTRY *entries,
    size_t entry_count,
    uint64_t address,
    uint64_t length)
{
    uint64_t end;
    uint64_t cursor = address;

    if (entries == 0 || entry_count == 0 || length == 0 ||
        address > UINT64_MAX - length)
        return 0;
    end = address + length;

    for (size_t i = 0; i < entry_count; ++i) {
        uint64_t entry_end;

        if (entries[i].length == 0 ||
            entries[i].base > UINT64_MAX - entries[i].length)
            return 0;
        entry_end = entries[i].base + entries[i].length;

        if (entry_end <= cursor)
            continue;
        if (entries[i].base > cursor ||
            !acpi_memory_type_is_readable(entries[i].type))
            return 0;

        cursor = entry_end < end ? entry_end : end;
        if (cursor == end)
            return 1;
    }

    return 0;
}

static int checksum_is_zero(
    const volatile uint8_t *bytes,
    uint64_t length)
{
    uint8_t sum = 0;

    for (uint64_t i = 0; i < length; ++i)
        sum = (uint8_t)(sum + bytes[i]);
    return sum == 0;
}

int vgk_acpi_rsdp_validate(
    const VGK_MEMORY_MAP_ENTRY *memory_map,
    size_t memory_map_count,
    uint64_t physical_address,
    const volatile uint8_t *bytes,
    uint64_t *length_out)
{
    uint64_t length = VGK_ACPI_RSDP_MIN_LENGTH;

    if (length_out != 0)
        *length_out = 0;

    if (physical_address == 0 || bytes == 0 || length_out == 0 ||
        !range_is_readable_acpi_memory(
            memory_map, memory_map_count, physical_address, length))
        return 0;

    if (bytes[0] != 'R' || bytes[1] != 'S' ||
        bytes[2] != 'D' || bytes[3] != ' ' ||
        bytes[4] != 'P' || bytes[5] != 'T' ||
        bytes[6] != 'R' || bytes[7] != ' ')
        return 0;

    if (!checksum_is_zero(bytes, VGK_ACPI_RSDP_MIN_LENGTH))
        return 0;

    /* ACPI 1.0 RSDP ends after the legacy 20-byte structure. */
    if (bytes[15] >= 2U) {
        if (!range_is_readable_acpi_memory(
                memory_map, memory_map_count, physical_address,
                VGK_ACPI_RSDP_EXTENDED_PREFIX))
            return 0;

        length = (uint64_t)bytes[20] |
                 ((uint64_t)bytes[21] << 8) |
                 ((uint64_t)bytes[22] << 16) |
                 ((uint64_t)bytes[23] << 24);
        if (length < 36U || length > VGK_ACPI_RSDP_MAX_LENGTH ||
            !range_is_readable_acpi_memory(
                memory_map, memory_map_count, physical_address, length))
            return 0;

        if (!checksum_is_zero(bytes, length))
            return 0;
    }

    *length_out = length;
    return 1;
}
