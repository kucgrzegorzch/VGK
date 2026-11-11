#include "uefi_memory_map.h"

#define VGK_PAGE_SIZE UINT64_C(4096)
#define VGK_EFI_MEMORY_DESCRIPTOR_VERSION UINT32_C(1)

static uint64_t normalize_type(uint32_t type)
{
    switch (type) {
    case 1:  /* EfiLoaderCode */
    case 2:  /* EfiLoaderData */
    case 3:  /* EfiBootServicesCode */
    case 4:  /* EfiBootServicesData */
        return VGK_MEMORY_BOOTLOADER;
    case 5:  /* EfiRuntimeServicesCode */
    case 6:  /* EfiRuntimeServicesData */
        return VGK_MEMORY_FIRMWARE;
    case 7:  /* EfiConventionalMemory */
        return VGK_MEMORY_USABLE;
    case 8:  /* EfiUnusableMemory */
        return VGK_MEMORY_BAD;
    case 9:  /* EfiACPIReclaimMemory */
        return VGK_MEMORY_ACPI_RECLAIM;
    case 10: /* EfiACPIMemoryNVS */
        return VGK_MEMORY_ACPI_NVS;
    case 0:  /* EfiReservedMemoryType */
    case 11: /* EfiMemoryMappedIO */
        return VGK_MEMORY_MMIO;
    case 12: /* EfiMemoryMappedIOPortSpace */
    case 13: /* EfiPalCode */
    case 14: /* EfiPersistentMemory */
    case 15: /* EfiUnacceptedMemoryType */
    default:
        return VGK_MEMORY_RESERVED;
    }
}

static void sort_by_address(VGK_MEMORY_MAP_ENTRY *entries, size_t count)
{
    for (size_t i = 1; i < count; ++i) {
        VGK_MEMORY_MAP_ENTRY current = entries[i];
        size_t j = i;
        while (j > 0 && entries[j - 1U].base > current.base) {
            entries[j] = entries[j - 1U];
            --j;
        }
        entries[j] = current;
    }
}

int vgk_uefi_normalize_memory_map(
    const void *firmware_map,
    size_t firmware_map_size,
    size_t descriptor_size,
    uint32_t descriptor_version,
    VGK_MEMORY_MAP_ENTRY *output,
    size_t output_capacity,
    size_t *output_count)
{
    const uint8_t *bytes = (const uint8_t *)firmware_map;
    size_t count;

    if (output_count == 0)
        return -1;
    *output_count = 0;

    if (firmware_map == 0 || output == 0 ||
        firmware_map_size == 0 ||
        descriptor_version != VGK_EFI_MEMORY_DESCRIPTOR_VERSION ||
        descriptor_size < sizeof(VGK_EFI_MEMORY_DESCRIPTOR) ||
        (descriptor_size & 7U) != 0 ||
        (firmware_map_size % descriptor_size) != 0 ||
        ((uintptr_t)firmware_map & 7U) != 0)
        return -1;

    count = firmware_map_size / descriptor_size;
    if (count == 0 || count > output_capacity)
        return -1;

    for (size_t i = 0; i < count; ++i) {
        VGK_EFI_MEMORY_DESCRIPTOR descriptor;
        uint64_t length;
        uint64_t end;

        /* Copy only the defined prefix; future firmware may extend the stride. */
        const uint8_t *source = bytes + i * descriptor_size;
        uint8_t *destination = (uint8_t *)&descriptor;
        for (size_t j = 0; j < sizeof(descriptor); ++j)
            destination[j] = source[j];

        if (descriptor.number_of_pages == 0 ||
            descriptor.number_of_pages > UINT64_MAX / VGK_PAGE_SIZE)
            return -1;

        length = descriptor.number_of_pages * VGK_PAGE_SIZE;
        if (descriptor.physical_start > UINT64_MAX - length)
            return -1;
        end = descriptor.physical_start + length;
        if (end <= descriptor.physical_start)
            return -1;

        output[i].base = descriptor.physical_start;
        output[i].length = length;
        output[i].type = normalize_type(descriptor.type);
        output[i].attributes = descriptor.attribute;
    }

    sort_by_address(output, count);

    for (size_t i = 1; i < count; ++i) {
        uint64_t previous_end = output[i - 1U].base + output[i - 1U].length;
        if (previous_end < output[i - 1U].base ||
            output[i].base < previous_end)
            return -1;
    }

    *output_count = count;
    return 0;
}
